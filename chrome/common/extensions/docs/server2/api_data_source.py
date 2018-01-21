# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import copy
import logging
import os
import posixpath

from data_source import DataSource
from docs_server_utils import StringIdentity, MarkFirstAndLast
from environment import IsPreviewServer, IsReleaseServer
from extensions_paths import JSON_TEMPLATES, PRIVATE_TEMPLATES
from file_system import FileNotFoundError
from future import Future, Collect
from operator import itemgetter
from platform_util import GetPlatforms
import third_party.json_schema_compiler.json_parse as json_parse
import third_party.json_schema_compiler.model as model
from third_party.json_schema_compiler.memoize import memoize


# The set of possible categories a node may belong to.
_NODE_CATEGORIES = ('types', 'functions', 'events', 'properties')


def _CreateId(node, prefix):
  if node.parent is not None and not isinstance(node.parent, model.Namespace):
    return '-'.join([prefix, node.parent.simple_name, node.simple_name])
  return '-'.join([prefix, node.simple_name])


def _FormatValue(value):
  '''Inserts commas every three digits for integer values. It is magic.
  '''
  s = str(value)
  return ','.join([s[max(0, i - 3):i] for i in range(len(s), 0, -3)][::-1])


def _GetByNameDict(namespace):
  '''Returns a dictionary mapping names to named items from |namespace|.

  This lets us render specific API entities rather than the whole thing at once,
  for example {{apis.manifestTypes.byName.ExternallyConnectable}}.

  Includes items from namespace['types'], namespace['functions'],
  namespace['events'], and namespace['properties'].
  '''
  by_name = {}
  for item_type in _NODE_CATEGORIES:
    if item_type in namespace:
      old_size = len(by_name)
      by_name.update(
          (item['name'], item) for item in namespace[item_type])
      assert len(by_name) == old_size + len(namespace[item_type]), (
          'Duplicate name in %r' % namespace)
  return by_name


def _GetEventByNameFromEvents(events):
  '''Parses the dictionary |events| to find the definitions of members of the
  type Event.  Returns a dictionary mapping the name of a member to that
  member's definition.
  '''
  assert 'types' in events, \
      'The dictionary |events| must contain the key "types".'
  event_list = [t for t in events['types'] if t.get('name') == 'Event']
  assert len(event_list) == 1, 'Exactly one type must be called "Event".'
  return _GetByNameDict(event_list[0])


class _APINodeCursor(object):
  '''An abstract representation of a node in an APISchemaGraph.
  The current position in the graph is represented by a path into the
  underlying dictionary. So if the APISchemaGraph is:

    {
      'tabs': {
        'types': {
          'Tab': {
            'properties': {
              'url': {
                ...
              }
            }
          }
        }
      }
    }

  then the 'url' property would be represented by:

    ['tabs', 'types', 'Tab', 'properties', 'url']
  '''

  def __init__(self, availability_finder, namespace_name):
    self._lookup_path = []
    self._node_availabilities = availability_finder.GetAPINodeAvailability(
        namespace_name)
    self._namespace_name = namespace_name
    self._ignored_categories = []

  def _AssertIsValidCategory(self, category):
    assert category in _NODE_CATEGORIES, \
        '%s is not a valid category. Full path: %s' % (category, str(self))

  def _GetParentPath(self):
    '''Returns the path pointing to this node's parent.
    '''
    assert len(self._lookup_path) > 1, \
        'Tried to look up parent for the top-level node.'

    # lookup_path[-1] is the name of the current node. If this lookup_path
    # describes a regular node, then lookup_path[-2] will be a node category.
    # Otherwise, it's an event callback or a function parameter.
    if self._lookup_path[-2] not in _NODE_CATEGORIES:
      if self._lookup_path[-1] == 'callback':
        # This is an event callback, so lookup_path[-2] is the event
        # node name, thus lookup_path[-3] must be 'events'.
        assert self._lookup_path[-3] == 'events'
        return self._lookup_path[:-1]
      # This is a function parameter.
      assert self._lookup_path[-2] == 'parameters'
      return self._lookup_path[:-2]
    # This is a regular node, so lookup_path[-2] should
    # be a node category.
    self._AssertIsValidCategory(self._lookup_path[-2])
    return self._lookup_path[:-2]

  def _LookupNodeAvailability(self, lookup_path):
    '''Returns the ChannelInfo object for this node.
    '''
    return self._node_availabilities.Lookup(self._namespace_name,
                                            *lookup_path).annotation

  def _CheckNamespacePrefix(self, lookup_path):
    '''API schemas may prepend the namespace name to top-level types
    (e.g. declarativeWebRequest > types > declarativeWebRequest.IgnoreRules),
    but just the base name (here, 'IgnoreRules') will be in the |lookup_path|.
    Try creating an alternate |lookup_path| by adding the namespace name.
    '''
    # lookup_path[0] is always the node category (e.g. types, functions, etc.).
    # Thus, lookup_path[1] is always the top-level node name.
    self._AssertIsValidCategory(lookup_path[0])
    base_name = lookup_path[1]
    lookup_path[1] = '%s.%s' % (self._namespace_name, base_name)
    try:
      node_availability = self._LookupNodeAvailability(lookup_path)
      if node_availability is not None:
        return node_availability
    finally:
      # Restore lookup_path.
      lookup_path[1] = base_name
    return None

  def _CheckEventCallback(self, lookup_path):
    '''Within API schemas, an event has a list of 'properties' that the event's
    callback expects. The callback itself is not explicitly represented in the
    schema. However, when creating an event node in _JSCModel, a callback node
    is generated and acts as the parent for the event's properties.
    Modify |lookup_path| to check the original schema format.
    '''
    if 'events' in lookup_path:
      assert 'callback' in lookup_path, self
      callback_index = lookup_path.index('callback')
      try:
        lookup_path.pop(callback_index)
        node_availability = self._LookupNodeAvailability(lookup_path)
      finally:
        lookup_path.insert(callback_index, 'callback')
      return node_availability
    return None

  def _LookupAvailability(self, lookup_path):
    '''Runs all the lookup checks on |lookup_path| and
    returns the node availability if found, None otherwise.
    '''
    for lookup in (self._LookupNodeAvailability,
                   self._CheckEventCallback,
                   self._CheckNamespacePrefix):
      node_availability = lookup(lookup_path)
      if node_availability is not None:
        return node_availability
    return None

  def _GetCategory(self):
    '''Returns the category this node belongs to.
    '''
    if self._lookup_path[-2] in _NODE_CATEGORIES:
      return self._lookup_path[-2]
    # If lookup_path[-2] is not in _NODE_CATEGORIES and
    # lookup_path[-1] is 'callback', then we know we have
    # an event callback.
    if self._lookup_path[-1] == 'callback':
      return 'events'
    if self._lookup_path[-2] == 'parameters':
      # Function parameters are modelled as properties.
      return 'properties'
    if (self._lookup_path[-1].endswith('Type') and
        (self._lookup_path[-1][:-len('Type')] == self._lookup_path[-2] or
         self._lookup_path[-1][:-len('ReturnType')] == self._lookup_path[-2])):
      # Array elements and function return objects have 'Type' and 'ReturnType'
      # appended to their names, respectively, in model.py. This results in
      # lookup paths like
      # 'events > types > Rule > properties > tags > tagsType'.
      # These nodes are treated as properties.
      return 'properties'
    if self._lookup_path[0] == 'events':
      # HACK(ahernandez.miralles): This catches a few edge cases,
      # such as 'webviewTag > events > consolemessage > level'.
      return 'properties'
    raise AssertionError('Could not classify node %s' % self)

  def GetDeprecated(self):
    '''Returns when this node became deprecated, or None if it
    is not deprecated.
    '''
    deprecated_path = self._lookup_path + ['deprecated']
    for lookup in (self._LookupNodeAvailability,
                   self._CheckNamespacePrefix):
      node_availability = lookup(deprecated_path)
      if node_availability is not None:
        return node_availability
    if 'callback' in self._lookup_path:
      return self._CheckEventCallback(deprecated_path)
    return None

  def GetAvailability(self):
    '''Returns availability information for this node.
    '''
    if self._GetCategory() in self._ignored_categories:
      return None
    node_availability = self._LookupAvailability(self._lookup_path)
    if node_availability is None:
      logging.warning('No availability found for: %s' % self)
      return None

    parent_node_availability = self._LookupAvailability(self._GetParentPath())
    # If the parent node availability couldn't be found, something
    # is very wrong.
    assert parent_node_availability is not None

    # Only render this node's availability if it differs from the parent
    # node's availability.
    if node_availability == parent_node_availability:
      return None
    return node_availability

  def Descend(self, *path, **kwargs):
    '''Moves down the APISchemaGraph, following |path|.
    |ignore| should be a tuple of category strings (e.g. ('types',))
    for which nodes should not have availability data generated.
    '''
    ignore = kwargs.get('ignore')
    class scope(object):
      def __enter__(self2):
        if ignore:
          self._ignored_categories.extend(ignore)
        if path:
          self._lookup_path.extend(path)

      def __exit__(self2, _, __, ___):
        if ignore:
          self._ignored_categories[:] = self._ignored_categories[:-len(ignore)]
        if path:
          self._lookup_path[:] = self._lookup_path[:-len(path)]
    return scope()

  def __str__(self):
    return repr(self)

  def __repr__(self):
    return '%s > %s' % (self._namespace_name, ' > '.join(self._lookup_path))


class _JSCModel(object):
  '''Uses a Model from the JSON Schema Compiler and generates a dict that
  a Handlebar template can use for a data source.
  '''

  def __init__(self,
               content_script_apis,
               namespace,
               availability_finder,
               json_cache,
               template_cache,
               features_bundle,
               event_byname_future):
    self._content_script_apis = content_script_apis
    self._availability = availability_finder.GetAPIAvailability(namespace.name)
    self._current_node = _APINodeCursor(availability_finder, namespace.name)
    self._api_availabilities = json_cache.GetFromFile(
        posixpath.join(JSON_TEMPLATES, 'api_availabilities.json'))
    self._intro_tables = json_cache.GetFromFile(
        posixpath.join(JSON_TEMPLATES, 'intro_tables.json'))
    self._api_features = features_bundle.GetAPIFeatures()
    self._template_cache = template_cache
    self._event_byname_future = event_byname_future
    self._namespace = namespace

  def _GetLink(self, link):
    ref = link if '.' in link else (self._namespace.name + '.' + link)
    return { 'ref': ref, 'text': link, 'name': link }

  def ToDict(self):
    if self._namespace is None:
      return {}
    chrome_dot_name = 'chrome.%s' % self._namespace.name
    as_dict = {
      'name': self._namespace.name,
      'namespace': self._namespace.documentation_options.get('namespace',
                                                             chrome_dot_name),
      'title': self._namespace.documentation_options.get('title',
                                                         chrome_dot_name),
      'documentationOptions': self._namespace.documentation_options,
      'types': self._GenerateTypes(self._namespace.types.values()),
      'functions': self._GenerateFunctions(self._namespace.functions),
      'events': self._GenerateEvents(self._namespace.events),
      'domEvents': self._GenerateDomEvents(self._namespace.events),
      'properties': self._GenerateProperties(self._namespace.properties),
      'introList': self._GetIntroTableList(),
      'channelWarning': self._GetChannelWarning(),
    }
    if self._namespace.deprecated:
      as_dict['deprecated'] = self._namespace.deprecated

    as_dict['byName'] = _GetByNameDict(as_dict)
    return as_dict

  def _GetChannelWarning(self):
    if not self._IsExperimental():
      return {
        self._availability.channel_info.channel: True
      }
    return None

  def _IsExperimental(self):
    return self._namespace.name.startswith('experimental')

  def _GenerateTypes(self, types):
    with self._current_node.Descend('types'):
      return [self._GenerateType(t) for t in types]

  def _GenerateType(self, type_):
    with self._current_node.Descend(type_.simple_name):
      type_dict = {
        'name': type_.simple_name,
        'description': type_.description,
        'properties': self._GenerateProperties(type_.properties),
        'functions': self._GenerateFunctions(type_.functions),
        'events': self._GenerateEvents(type_.events),
        'id': _CreateId(type_, 'type'),
        'availability': self._GetAvailabilityTemplate()
      }
      self._RenderTypeInformation(type_, type_dict)
      return type_dict

  def _GenerateFunctions(self, functions):
    with self._current_node.Descend('functions'):
      return [self._GenerateFunction(f) for f in functions.values()]

  def _GenerateFunction(self, function):
    # When ignoring types, properties must be ignored as well.
    with self._current_node.Descend(function.simple_name,
                                    ignore=('types', 'properties')):
      function_dict = {
        'name': function.simple_name,
        'description': function.description,
        'callback': self._GenerateCallback(function.callback),
        'parameters': [],
        'returns': None,
        'id': _CreateId(function, 'method'),
        'availability': self._GetAvailabilityTemplate()
      }
      self._AddCommonProperties(function_dict, function)
      if function.returns:
        function_dict['returns'] = self._GenerateType(function.returns)
    with self._current_node.Descend(function.simple_name):
      with self._current_node.Descend('parameters'):
        for param in function.params:
          function_dict['parameters'].append(self._GenerateProperty(param))
      if function.callback is not None:
        # Show the callback as an extra parameter.
        function_dict['parameters'].append(
            self._GenerateCallbackProperty(function.callback))
      if len(function_dict['parameters']) > 0:
        function_dict['parameters'][-1]['last'] = True
      return function_dict

  def _GenerateEvents(self, events):
    with self._current_node.Descend('events'):
      return [self._GenerateEvent(e) for e in events.values()
              if not e.supports_dom]

  def _GenerateDomEvents(self, events):
    with self._current_node.Descend('events'):
      return [self._GenerateEvent(e) for e in events.values()
              if e.supports_dom]

  def _GenerateEvent(self, event):
    with self._current_node.Descend(event.simple_name, ignore=('properties',)):
      event_dict = {
        'name': event.simple_name,
        'description': event.description,
        'filters': [self._GenerateProperty(f) for f in event.filters],
        'conditions': [self._GetLink(condition)
                       for condition in event.conditions],
        'actions': [self._GetLink(action) for action in event.actions],
        'supportsRules': event.supports_rules,
        'supportsListeners': event.supports_listeners,
        'properties': [],
        'id': _CreateId(event, 'event'),
        'byName': {},
        'availability': self._GetAvailabilityTemplate()
      }
    with self._current_node.Descend(event.simple_name):
      self._AddCommonProperties(event_dict, event)
      # Add the Event members to each event in this object.
      if self._event_byname_future:
        event_dict['byName'].update(self._event_byname_future.Get())
      # We need to create the method description for addListener based on the
      # information stored in |event|.
      if event.supports_listeners:
        callback_object = model.Function(parent=event,
                                         name='callback',
                                         json={},
                                         namespace=event.parent,
                                         origin='')
        callback_object.params = event.params
        if event.callback:
          callback_object.callback = event.callback
        callback_parameters = self._GenerateCallbackProperty(callback_object)
        callback_parameters['last'] = True
        event_dict['byName']['addListener'] = {
          'name': 'addListener',
          'callback': self._GenerateFunction(callback_object),
          'parameters': [callback_parameters]
        }
    with self._current_node.Descend(event.simple_name, ignore=('properties',)):
      if event.supports_dom:
        # Treat params as properties of the custom Event object associated with
        # this DOM Event.
        event_dict['properties'] += [self._GenerateProperty(param)
                                     for param in event.params]
      return event_dict

  def _GenerateCallback(self, callback):
    if not callback:
      return None
    callback_dict = {
      'name': callback.simple_name,
      'simple_type': {'simple_type': 'function'},
      'optional': callback.optional,
      'parameters': []
    }
    with self._current_node.Descend('parameters',
                                    callback.simple_name,
                                    'parameters'):
      for param in callback.params:
        callback_dict['parameters'].append(self._GenerateProperty(param))
    if (len(callback_dict['parameters']) > 0):
      callback_dict['parameters'][-1]['last'] = True
    return callback_dict

  def _GenerateProperties(self, properties):
    with self._current_node.Descend('properties'):
      return [self._GenerateProperty(v) for v in properties.values()]

  def _GenerateProperty(self, property_):
    with self._current_node.Descend(property_.simple_name):
      if not hasattr(property_, 'type_'):
        for d in dir(property_):
          if not d.startswith('_'):
            print ('%s -> %s' % (d, getattr(property_, d)))
      type_ = property_.type_

      # Make sure we generate property info for arrays, too.
      # TODO(kalman): what about choices?
      if type_.property_type == model.PropertyType.ARRAY:
        properties = type_.item_type.properties
      else:
        properties = type_.properties

      property_dict = {
        'name': property_.simple_name,
        'optional': property_.optional,
        'description': property_.description,
        'properties': self._GenerateProperties(type_.properties),
        'functions': self._GenerateFunctions(type_.functions),
        'parameters': [],
        'returns': None,
        'id': _CreateId(property_, 'property'),
        'availability': self._GetAvailabilityTemplate()
      }
      self._AddCommonProperties(property_dict, property_)

      if type_.property_type == model.PropertyType.FUNCTION:
        function = type_.function
        with self._current_node.Descend('parameters'):
          for param in function.params:
            property_dict['parameters'].append(self._GenerateProperty(param))
        if function.returns:
          with self._current_node.Descend(ignore=('types', 'properties')):
            property_dict['returns'] = self._GenerateType(function.returns)

      value = property_.value
      if value is not None:
        if isinstance(value, int):
          property_dict['value'] = _FormatValue(value)
        else:
          property_dict['value'] = value
      else:
        self._RenderTypeInformation(type_, property_dict)

      return property_dict

  def _GenerateCallbackProperty(self, callback):
    property_dict = {
      'name': callback.simple_name,
      'description': callback.description,
      'optional': callback.optional,
      'is_callback': True,
      'id': _CreateId(callback, 'property'),
      'simple_type': 'function',
    }
    if (callback.parent is not None and
        not isinstance(callback.parent, model.Namespace)):
      property_dict['parentName'] = callback.parent.simple_name
    return property_dict

  def _RenderTypeInformation(self, type_, dst_dict):
    with self._current_node.Descend(ignore=('types', 'properties')):
      dst_dict['is_object'] = type_.property_type == model.PropertyType.OBJECT
      if type_.property_type == model.PropertyType.CHOICES:
        dst_dict['choices'] = self._GenerateTypes(type_.choices)
        # We keep track of which == last for knowing when to add "or" between
        # choices in templates.
        if len(dst_dict['choices']) > 0:
          dst_dict['choices'][-1]['last'] = True
      elif type_.property_type == model.PropertyType.REF:
        dst_dict['link'] = self._GetLink(type_.ref_type)
      elif type_.property_type == model.PropertyType.ARRAY:
        dst_dict['array'] = self._GenerateType(type_.item_type)
      elif type_.property_type == model.PropertyType.ENUM:
        dst_dict['enum_values'] = [
            {'name': value.name, 'description': value.description}
            for value in type_.enum_values]
        if len(dst_dict['enum_values']) > 0:
          dst_dict['enum_values'][-1]['last'] = True
      elif type_.instance_of is not None:
        dst_dict['simple_type'] = type_.instance_of
      else:
        dst_dict['simple_type'] = type_.property_type.name

  def _GetIntroTableList(self):
    '''Create a generic data structure that can be traversed by the templates
    to create an API intro table.
    '''
    intro_rows = [
      self._GetIntroDescriptionRow(),
      self._GetIntroAvailabilityRow()
    ] + self._GetIntroDependencyRows() + self._GetIntroContentScriptRow()

    # Add rows using data from intro_tables.json, overriding any existing rows
    # if they share the same 'title' attribute.
    row_titles = [row['title'] for row in intro_rows]
    for misc_row in self._GetMiscIntroRows():
      if misc_row['title'] in row_titles:
        intro_rows[row_titles.index(misc_row['title'])] = misc_row
      else:
        intro_rows.append(misc_row)

    return intro_rows

  def _CreateAvailabilityTemplate(self, status, scheduled, version):
    '''Returns an object suitable for use in templates to display availability
    information.
    '''
    return {
      'partial': self._template_cache.GetFromFile(
          '%sintro_tables/%s_message.html' % (PRIVATE_TEMPLATES, status)).Get(),
      'scheduled': scheduled,
      'version': version
    }

  def _GetIntroContentScriptRow(self):
    content_script_support = self._content_script_apis.get(self._namespace.name)
    if content_script_support is None:
      return []
    if content_script_support.restrictedTo:
      content_script_support.restrictedTo.sort(key=itemgetter('node'))
      MarkFirstAndLast(content_script_support.restrictedTo)
    return [{
      'title': 'Content Scripts',
      'content': [{
        'partial': self._template_cache.GetFromFile(
            posixpath.join(PRIVATE_TEMPLATES,
                           'intro_tables',
                           'content_scripts.html')).Get(),
        'contentScriptSupport': content_script_support.__dict__
      }]
    }]
  def _GetAvailabilityTemplate(self):
    '''Gets availability for the current node and returns an appropriate
    template object.
    '''
    # Displaying deprecated status takes precedence over when the API
    # became stable.
    availability_info = self._current_node.GetDeprecated()
    if availability_info is not None:
      status = 'deprecated'
    else:
      availability_info = self._current_node.GetAvailability()
      if availability_info is None:
        return None
      status = availability_info.channel_info.channel
    return self._CreateAvailabilityTemplate(
        status,
        availability_info.scheduled,
        availability_info.channel_info.version)

  def _GetIntroDescriptionRow(self):
    ''' Generates the 'Description' row data for an API intro table.
    '''
    return {
      'title': 'Description',
      'content': [
        { 'text': self._namespace.description }
      ]
    }

  def _GetIntroAvailabilityRow(self):
    ''' Generates the 'Availability' row data for an API intro table.
    '''
    if self._IsExperimental():
      status = 'experimental'
      scheduled = None
      version = None
    else:
      status = self._availability.channel_info.channel
      scheduled = self._availability.scheduled
      version = self._availability.channel_info.version
    return {
      'title': 'Availability',
      'content': [
        self._CreateAvailabilityTemplate(status, scheduled, version)
      ]
    }

  def _GetIntroDependencyRows(self):
    # Devtools aren't in _api_features. If we're dealing with devtools, bail.
    if 'devtools' in self._namespace.name:
      return []

    api_feature = self._api_features.Get().get(self._namespace.name)
    if not api_feature:
      logging.error('"%s" not found in _api_features.json' %
                    self._namespace.name)
      return []

    permissions_content = []
    manifest_content = []

    def categorize_dependency(dependency):
      def make_code_node(text):
        return { 'class': 'code', 'text': text }

      context, name = dependency.split(':', 1)
      if context == 'permission':
        permissions_content.append(make_code_node('"%s"' % name))
      elif context == 'manifest':
        manifest_content.append(make_code_node('"%s": {...}' % name))
      elif context == 'api':
        transitive_dependencies = (
            self._api_features.Get().get(name, {}).get('dependencies', []))
        for transitive_dependency in transitive_dependencies:
          categorize_dependency(transitive_dependency)
      else:
        logging.error('Unrecognized dependency for %s: %s' %
                      (self._namespace.name, context))

    for dependency in api_feature.get('dependencies', ()):
      categorize_dependency(dependency)

    dependency_rows = []
    if permissions_content:
      dependency_rows.append({
        'title': 'Permissions',
        'content': permissions_content
      })
    if manifest_content:
      dependency_rows.append({
        'title': 'Manifest',
        'content': manifest_content
      })
    return dependency_rows

  def _GetMiscIntroRows(self):
    ''' Generates miscellaneous intro table row data, such as 'Permissions',
    'Samples', and 'Learn More', using intro_tables.json.
    '''
    misc_rows = []
    # Look up the API name in intro_tables.json, which is structured
    # similarly to the data structure being created. If the name is found, loop
    # through the attributes and add them to this structure.
    table_info = self._intro_tables.Get().get(self._namespace.name)
    if table_info is None:
      return misc_rows

    for category in table_info.iterkeys():
      content = []
      for node in table_info[category]:
        # If there is a 'partial' argument and it hasn't already been
        # converted to a Handlebar object, transform it to a template.
        if 'partial' in node:
          # Note: it's enough to copy() not deepcopy() because only a single
          # top-level key is being modified.
          node = copy(node)
          node['partial'] = self._template_cache.GetFromFile(
              posixpath.join(PRIVATE_TEMPLATES, node['partial'])).Get()
        content.append(node)
      misc_rows.append({ 'title': category, 'content': content })
    return misc_rows

  def _AddCommonProperties(self, target, src):
    if src.deprecated is not None:
      target['deprecated'] = src.deprecated
    if (src.parent is not None and
        not isinstance(src.parent, model.Namespace)):
      target['parentName'] = src.parent.simple_name


class _LazySamplesGetter(object):
  '''This class is needed so that an extensions API page does not have to fetch
  the apps samples page and vice versa.
  '''

  def __init__(self, api_name, samples):
    self._api_name = api_name
    self._samples = samples

  def get(self, key):
    return self._samples.FilterSamples(key, self._api_name)


class APIDataSource(DataSource):
  '''This class fetches and loads JSON APIs from the FileSystem passed in with
  |compiled_fs_factory|, so the APIs can be plugged into templates.
  '''
  def __init__(self, server_instance, request):
    file_system = server_instance.host_file_system_provider.GetTrunk()
    self._json_cache = server_instance.compiled_fs_factory.ForJson(file_system)
    self._template_cache = server_instance.compiled_fs_factory.ForTemplates(
        file_system)
    self._platform_bundle = server_instance.platform_bundle
    self._model_cache = server_instance.object_store_creator.Create(
        APIDataSource,
        # Update the models when any of templates, APIs, or Features change.
        category=StringIdentity(self._json_cache.GetIdentity(),
                                self._template_cache.GetIdentity(),
                                self._platform_bundle.GetIdentity()))

    # This caches the result of _LoadEventByName.
    self._event_byname_futures = {}
    self._samples = server_instance.samples_data_source_factory.Create(request)

  def _LoadEventByName(self, platform):
    '''All events have some members in common. We source their description
    from Event in events.json.
    '''
    if platform not in self._event_byname_futures:
      future = self._GetSchemaModel(platform, 'events')
      self._event_byname_futures[platform] = Future(
          callback=lambda: _GetEventByNameFromEvents(future.Get()))
    return self._event_byname_futures[platform]

  def _GetSchemaModel(self, platform, api_name):
    object_store_key = '/'.join((platform, api_name))
    api_models = self._platform_bundle.GetAPIModels(platform)
    jsc_model_future = self._model_cache.Get(object_store_key)
    model_future = api_models.GetModel(api_name)
    content_script_apis_future = api_models.GetContentScriptAPIs()
    def resolve():
      jsc_model = jsc_model_future.Get()
      if jsc_model is None:
        jsc_model = _JSCModel(
            content_script_apis_future.Get(),
            model_future.Get(),
            self._platform_bundle.GetAvailabilityFinder(platform),
            self._json_cache,
            self._template_cache,
            self._platform_bundle.GetFeaturesBundle(platform),
            self._LoadEventByName(platform)).ToDict()
        self._model_cache.Set(object_store_key, jsc_model)
      return jsc_model
    return Future(callback=resolve)

  def _GetImpl(self, platform, api_name):
    handlebar_dict_future = self._GetSchemaModel(platform, api_name)
    def resolve():
      handlebar_dict = handlebar_dict_future.Get()
      # Parsing samples on the preview server takes seconds and doesn't add
      # anything. Don't do it.
      if not IsPreviewServer():
        handlebar_dict['samples'] = _LazySamplesGetter(
            handlebar_dict['name'],
            self._samples)
      return handlebar_dict
    return Future(callback=resolve)

  def get(self, platform):
    '''Return a getter object so that templates can perform lookups such
    as apis.extensions.runtime.
    '''
    getter = lambda: 0
    getter.get = lambda api_name: self._GetImpl(platform, api_name).Get()
    return getter

  def Cron(self):
    futures = []
    for platform in GetPlatforms():
      futures += [self._GetImpl(platform, name)
          for name in self._platform_bundle.GetAPIModels(platform).GetNames()]
    return Collect(futures, except_pass=FileNotFoundError)
