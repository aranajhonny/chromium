// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/file_template.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/value_extractors.h"

namespace functions {

const char kProcessFileTemplate[] = "process_file_template";
const char kProcessFileTemplate_HelpShort[] =
    "process_file_template: Do template expansion over a list of files.";
const char kProcessFileTemplate_Help[] =
    "process_file_template: Do template expansion over a list of files.\n"
    "\n"
    "  process_file_template(source_list, template)\n"
    "\n"
    "  process_file_template applies a template list to a source file list,\n"
    "  returning the result of applying each template to each source. This is\n"
    "  typically used for computing output file names from input files.\n"
    "\n"
    "  In most cases, get_target_outputs() will give the same result with\n"
    "  shorter, more maintainable code. This function should only be used\n"
    "  when that function can't be used (like there's no target or the target\n"
    "  is defined in another build file).\n"
    "\n"
    "Arguments:\n"
    "\n"
    "  The source_list is a list of file names.\n"
    "\n"
    "  The template can be a string or a list. If it is a list, multiple\n"
    "  output strings are generated for each input.\n"
    "\n"
    "  The following template substrings are used in the template arguments\n"
    "  and are replaced with the corresponding part of the input file name:\n"
    "\n"
    "    {{source}}\n"
    "        The entire source name.\n"
    "\n"
    "    {{source_name_part}}\n"
    "        The source name with no path or extension.\n"
    "\n"
    "Example:\n"
    "\n"
    "  sources = [\n"
    "    \"foo.idl\",\n"
    "    \"bar.idl\",\n"
    "  ]\n"
    "  myoutputs = process_file_template(\n"
    "      sources,\n"
    "      [ \"$target_gen_dir/{{source_name_part}}.cc\",\n"
    "        \"$target_gen_dir/{{source_name_part}}.h\" ])\n"
    "\n"
    " The result in this case will be:\n"
    "    [ \"//out/Debug/foo.cc\"\n"
    "      \"//out/Debug/foo.h\"\n"
    "      \"//out/Debug/bar.cc\"\n"
    "      \"//out/Debug/bar.h\" ]\n";

Value RunProcessFileTemplate(Scope* scope,
                             const FunctionCallNode* function,
                             const std::vector<Value>& args,
                             Err* err) {
  if (args.size() != 2) {
    *err = Err(function->function(), "Expected two arguments");
    return Value();
  }

  FileTemplate file_template(scope->settings(), args[1],
                             FileTemplate::OUTPUT_ABSOLUTE, SourceDir(), err);
  if (err->has_error())
    return Value();

  Target::FileList input_files;
  if (!ExtractListOfRelativeFiles(scope->settings()->build_settings(), args[0],
                                  scope->GetSourceDir(), &input_files, err))
    return Value();

  Value ret(function, Value::LIST);

  // Temporary holding place, allocate outside to re-use buffer.
  std::vector<std::string> string_output;

  for (size_t i = 0; i < input_files.size(); i++) {
    string_output.clear();
    file_template.Apply(input_files[i], &string_output);
    for (size_t out_i = 0; out_i < string_output.size(); out_i++)
      ret.list_value().push_back(Value(function, string_output[out_i]));
  }
  return ret;
}

}  // namespace functions
