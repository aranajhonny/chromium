// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"

#include <ctype.h>

#include <map>
#include <string>
#include <vector>

#include "ash/system/chromeos/network/network_connect.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/net/onc_utils.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/options/network_property_ui_data.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/ui/mobile_config_ui.h"
#include "chrome/browser/chromeos/ui_proxy_config_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler_strings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_ip_config.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/ash_resources.h"
#include "grit/ui_chromeos_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/network/network_icon.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace options {

namespace {

// Keys for the network description dictionary passed to the web ui. Make sure
// to keep the strings in sync with what the JavaScript side uses.
const char kNetworkInfoKeyIconURL[] = "iconURL";
const char kNetworkInfoKeyServicePath[] = "servicePath";
const char kNetworkInfoKeyPolicyManaged[] = "policyManaged";

// These are keys for getting IP information from the web ui.
const char kIpConfigAddress[] = "address";
const char kIpConfigPrefixLength[] = "prefixLength";
const char kIpConfigNetmask[] = "netmask";
const char kIpConfigGateway[] = "gateway";
const char kIpConfigNameServers[] = "nameServers";
const char kIpConfigAutoConfig[] = "ipAutoConfig";
const char kIpConfigWebProxyAutoDiscoveryUrl[] = "webProxyAutoDiscoveryUrl";

// These are types of name server selections from the web ui.
const char kNameServerTypeAutomatic[] = "automatic";
const char kNameServerTypeGoogle[] = "google";
const char kNameServerTypeUser[] = "user";

// These are dictionary names used to send data to the web ui.
const char kDictionaryIpConfig[] = "ipconfig";
const char kDictionaryStaticIp[] = "staticIP";
const char kDictionarySavedIp[] = "savedIP";

// Google public name servers (DNS).
const char kGoogleNameServers[] = "8.8.4.4,8.8.8.8";

// Functions we call in JavaScript.
const char kRefreshNetworkDataFunction[] =
    "options.network.NetworkList.refreshNetworkData";
const char kSetDefaultNetworkIconsFunction[] =
    "options.network.NetworkList.setDefaultNetworkIcons";
const char kShowDetailedInfoFunction[] =
    "options.internet.DetailsInternetPage.showDetailedInfo";
const char kUpdateConnectionDataFunction[] =
    "options.internet.DetailsInternetPage.updateConnectionData";
const char kUpdateCarrierFunction[] =
    "options.internet.DetailsInternetPage.updateCarrier";
const char kUpdateSecurityTabFunction[] =
    "options.internet.DetailsInternetPage.updateSecurityTab";

// These are used to register message handlers with JavaScript.
const char kBuyDataPlanMessage[] = "buyDataPlan";
const char kChangePinMessage[] = "changePin";
const char kDisableCellularMessage[] = "disableCellular";
const char kDisableWifiMessage[] = "disableWifi";
const char kDisableWimaxMessage[] = "disableWimax";
const char kEnableCellularMessage[] = "enableCellular";
const char kEnableWifiMessage[] = "enableWifi";
const char kEnableWimaxMessage[] = "enableWimax";
const char kNetworkCommandMessage[] = "networkCommand";
const char kRefreshNetworksMessage[] = "refreshNetworks";
const char kSetApnMessage[] = "setApn";
const char kSetAutoConnectMessage[] = "setAutoConnect";
const char kSetCarrierMessage[] = "setCarrier";
const char kSetIPConfigMessage[] = "setIPConfig";
const char kSetPreferNetworkMessage[] = "setPreferNetwork";
const char kSetServerHostname[] = "setServerHostname";
const char kSetSimCardLockMessage[] = "setSimCardLock";
const char kShowMorePlanInfoMessage[] = "showMorePlanInfo";

// These are strings used to communicate with JavaScript.
const char kTagActivate[] = "activate";
const char kTagActivationState[] = "activationState";
const char kTagAddConnection[] = "add";
const char kTagApn[] = "apn";
const char kTagAutoConnect[] = "autoConnect";
const char kTagCarrierSelectFlag[] = "showCarrierSelect";
const char kTagCarrierUrl[] = "carrierUrl";
const char kTagCellularAvailable[] = "cellularAvailable";
const char kTagCellularEnabled[] = "cellularEnabled";
const char kTagCellularSupportsScan[] = "cellularSupportsScan";
const char kTagConfigure[] = "configure";
const char kTagConnect[] = "connect";
const char kTagControlledBy[] = "controlledBy";
const char kTagDeviceConnected[] = "deviceConnected";
const char kTagDisconnect[] = "disconnect";
const char kTagErrorState[] = "errorState";
const char kTagForget[] = "forget";
const char kTagIdentity[] = "identity";
const char kTagLanguage[] = "language";
const char kTagLastGoodApn[] = "lastGoodApn";
const char kTagLocalizedName[] = "localizedName";
const char kTagName[] = "name";
const char kTagNameServersGoogle[] = "nameServersGoogle";
const char kTagNameServerType[] = "nameServerType";
const char kTagNetworkId[] = "networkId";
const char kTagOptions[] = "options";
const char kTagPassword[] = "password";
const char kTagPolicy[] = "policy";
const char kTagPreferred[] = "preferred";
const char kTagProviderType[] = "providerType";
const char kTagProviderApnList[] = "providerApnList";
const char kTagRecommended[] = "recommended";
const char kTagRecommendedValue[] = "recommendedValue";
const char kTagRemembered[] = "remembered";
const char kTagRememberedList[] = "rememberedList";
const char kTagRestrictedPool[] = "restrictedPool";
const char kTagRoamingState[] = "roamingState";
const char kTagServerHostname[] = "serverHostname";
const char kTagCarriers[] = "carriers";
const char kTagCurrentCarrierIndex[] = "currentCarrierIndex";
const char kTagShared[] = "shared";
const char kTagShowActivateButton[] = "showActivateButton";
const char kTagShowStaticIPConfig[] = "showStaticIPConfig";
const char kTagShowViewAccountButton[] = "showViewAccountButton";
const char kTagSimCardLockEnabled[] = "simCardLockEnabled";
const char kTagSupportUrl[] = "supportUrl";
const char kTagTrue[] = "true";
const char kTagUsername[] = "username";
const char kTagValue[] = "value";
const char kTagVpnList[] = "vpnList";
const char kTagWifiAvailable[] = "wifiAvailable";
const char kTagWifiEnabled[] = "wifiEnabled";
const char kTagWimaxAvailable[] = "wimaxAvailable";
const char kTagWimaxEnabled[] = "wimaxEnabled";
const char kTagWiredList[] = "wiredList";
const char kTagWirelessList[] = "wirelessList";

const int kPreferredPriority = 1;

void ShillError(const std::string& function,
                const std::string& error_name,
                scoped_ptr<base::DictionaryValue> error_data) {
  // UpdateConnectionData may send requests for stale services; ignore
  // these errors.
  if (function == "UpdateConnectionData" &&
      error_name == network_handler::kDBusFailedError)
    return;
  NET_LOG_ERROR("Shill Error from InternetOptionsHandler: " + error_name,
                function);
}

const NetworkState* GetNetworkState(const std::string& service_path) {
  return NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
}

void SetNetworkProperty(const std::string& service_path,
                        const std::string& property,
                        base::Value* value) {
  NET_LOG_EVENT("SetNetworkProperty: " + property, service_path);
  base::DictionaryValue properties;
  properties.SetWithoutPathExpansion(property, value);
  NetworkHandler::Get()->network_configuration_handler()->SetProperties(
      service_path, properties,
      base::Bind(&base::DoNothing),
      base::Bind(&ShillError, "SetNetworkProperty"));
}

// Builds a dictionary with network information and an icon used for the
// NetworkList on the settings page. Ownership of the returned pointer is
// transferred to the caller.
base::DictionaryValue* BuildNetworkDictionary(
    const NetworkState* network,
    float icon_scale_factor,
    const PrefService* profile_prefs) {
  scoped_ptr<base::DictionaryValue> network_info =
      network_util::TranslateNetworkStateToONC(network);

  bool has_policy = onc::HasPolicyForNetwork(
      profile_prefs, g_browser_process->local_state(), *network);
  network_info->SetBoolean(kNetworkInfoKeyPolicyManaged, has_policy);

  std::string icon_url = ui::network_icon::GetImageUrlForNetwork(
      network, ui::network_icon::ICON_TYPE_LIST, icon_scale_factor);

  network_info->SetString(kNetworkInfoKeyIconURL, icon_url);
  network_info->SetString(kNetworkInfoKeyServicePath, network->path());

  return network_info.release();
}

// Pulls IP information out of a shill service properties dictionary. If
// |static_ip| is true, then it fetches "StaticIP.*" properties. If not, then it
// fetches "SavedIP.*" properties. Caller must take ownership of returned
// dictionary.  If non-NULL, |ip_parameters_set| returns a count of the number
// of IP routing parameters that get set.
base::DictionaryValue* BuildIPInfoDictionary(
    const base::DictionaryValue& shill_properties,
    bool static_ip,
    int* routing_parameters_set) {
  std::string address_key;
  std::string prefix_len_key;
  std::string gateway_key;
  std::string name_servers_key;
  if (static_ip) {
    address_key = shill::kStaticIPAddressProperty;
    prefix_len_key = shill::kStaticIPPrefixlenProperty;
    gateway_key = shill::kStaticIPGatewayProperty;
    name_servers_key = shill::kStaticIPNameServersProperty;
  } else {
    address_key = shill::kSavedIPAddressProperty;
    prefix_len_key = shill::kSavedIPPrefixlenProperty;
    gateway_key = shill::kSavedIPGatewayProperty;
    name_servers_key = shill::kSavedIPNameServersProperty;
  }

  scoped_ptr<base::DictionaryValue> ip_info_dict(new base::DictionaryValue);
  std::string address;
  int routing_parameters = 0;
  if (shill_properties.GetStringWithoutPathExpansion(address_key, &address)) {
    ip_info_dict->SetString(kIpConfigAddress, address);
    VLOG(2) << "Found " << address_key << ": " << address;
    routing_parameters++;
  }
  int prefix_len = -1;
  if (shill_properties.GetIntegerWithoutPathExpansion(
      prefix_len_key, &prefix_len)) {
    ip_info_dict->SetInteger(kIpConfigPrefixLength, prefix_len);
    std::string netmask = network_util::PrefixLengthToNetmask(prefix_len);
    ip_info_dict->SetString(kIpConfigNetmask, netmask);
    VLOG(2) << "Found " << prefix_len_key << ": "
            <<  prefix_len << " (" << netmask << ")";
    routing_parameters++;
  }
  std::string gateway;
  if (shill_properties.GetStringWithoutPathExpansion(gateway_key, &gateway)) {
    ip_info_dict->SetString(kIpConfigGateway, gateway);
    VLOG(2) << "Found " << gateway_key << ": " << gateway;
    routing_parameters++;
  }
  if (routing_parameters_set)
    *routing_parameters_set = routing_parameters;

  std::string name_servers;
  if (shill_properties.GetStringWithoutPathExpansion(
      name_servers_key, &name_servers)) {
    ip_info_dict->SetString(kIpConfigNameServers, name_servers);
    VLOG(2) << "Found " << name_servers_key << ": " << name_servers;
  }

  return ip_info_dict.release();
}

// Decorate dictionary |value_dict| with policy information from |ui_data|.
void DecorateValueDictionary(const NetworkPropertyUIData& ui_data,
                             const base::Value& value,
                             base::DictionaryValue* value_dict) {
  const base::Value* recommended_value = ui_data.default_value();
  if (ui_data.IsManaged())
    value_dict->SetString(kTagControlledBy, kTagPolicy);
  else if (recommended_value && recommended_value->Equals(&value))
    value_dict->SetString(kTagControlledBy, kTagRecommended);

  if (recommended_value)
    value_dict->Set(kTagRecommendedValue, recommended_value->DeepCopy());
}

// Decorate pref value as CoreOptionsHandler::CreateValueForPref() does and
// store it under |key| in |settings|. Takes ownership of |value|.
void SetValueDictionary(base::DictionaryValue* settings,
                        const char* key,
                        base::Value* value,
                        const NetworkPropertyUIData& ui_data) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // DictionaryValue::Set() takes ownership of |value|.
  dict->Set(kTagValue, value);
  settings->Set(key, dict);
  DecorateValueDictionary(ui_data, *value, dict);
}

// Creates a decorated dictionary like SetValueDictionary does, but extended for
// the Autoconnect property, which respects additionally global network policy.
void SetAutoconnectValueDictionary(bool network_is_private,
                                   ::onc::ONCSource onc_source,
                                   bool current_autoconnect,
                                   const NetworkPropertyUIData& ui_data,
                                   base::DictionaryValue* settings) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::Value* value = new base::FundamentalValue(current_autoconnect);
  // DictionaryValue::Set() takes ownership of |value|.
  dict->Set(kTagValue, value);
  settings->Set(kTagAutoConnect, dict);
  if (onc_source != ::onc::ONC_SOURCE_USER_POLICY &&
      onc_source != ::onc::ONC_SOURCE_DEVICE_POLICY) {
    // Autoconnect can be controlled by the GlobalNetworkConfiguration of the
    // ONC policy.
    bool only_policy_autoconnect =
        onc::PolicyAllowsOnlyPolicyNetworksToAutoconnect(network_is_private);
    if (only_policy_autoconnect) {
      dict->SetString(kTagControlledBy, kTagPolicy);
      return;
    }
  }
  DecorateValueDictionary(ui_data, *value, dict);
}

std::string CopyStringFromDictionary(const base::DictionaryValue& source,
                                     const std::string& src_key,
                                     const std::string& dest_key,
                                     base::DictionaryValue* dest) {
  std::string string_value;
  if (source.GetStringWithoutPathExpansion(src_key, &string_value))
    dest->SetStringWithoutPathExpansion(dest_key, string_value);
  return string_value;
}

// Fills |dictionary| with the configuration details of |vpn|. |onc| is required
// for augmenting the policy-managed information.
void PopulateVPNDetails(const NetworkState* vpn,
                        const base::DictionaryValue& shill_properties,
                        base::DictionaryValue* dictionary) {
  // Name and Remembered are set in PopulateConnectionDetails().
  // Provider properties are stored in the "Provider" dictionary.
  const base::DictionaryValue* provider_properties = NULL;
  if (!shill_properties.GetDictionaryWithoutPathExpansion(
          shill::kProviderProperty, &provider_properties)) {
    LOG(ERROR) << "No provider properties for VPN: " << vpn->path();
    return;
  }
  std::string provider_type;
  provider_properties->GetStringWithoutPathExpansion(
      shill::kTypeProperty, &provider_type);
  dictionary->SetString(kTagProviderType,
                        internet_options_strings::ProviderTypeString(
                            provider_type,
                            *provider_properties));

  std::string username;
  if (provider_type == shill::kProviderOpenVpn) {
    provider_properties->GetStringWithoutPathExpansion(
        shill::kOpenVPNUserProperty, &username);
  } else {
    provider_properties->GetStringWithoutPathExpansion(
        shill::kL2tpIpsecUserProperty, &username);
  }
  dictionary->SetString(kTagUsername, username);

  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  const base::DictionaryValue* onc =
      onc::FindPolicyForActiveUser(vpn->guid(), &onc_source);

  NetworkPropertyUIData hostname_ui_data;
  hostname_ui_data.ParseOncProperty(
      onc_source,
      onc,
      ::onc::network_config::VpnProperty(::onc::vpn::kHost));
  std::string provider_host;
  provider_properties->GetStringWithoutPathExpansion(
      shill::kHostProperty, &provider_host);
  SetValueDictionary(dictionary, kTagServerHostname,
                     new base::StringValue(provider_host),
                     hostname_ui_data);
}

// Given a list of supported carrier's by the device, return the index of
// the carrier the device is currently using.
int FindCurrentCarrierIndex(const base::ListValue* carriers,
                            const DeviceState* device) {
  DCHECK(carriers);
  DCHECK(device);
  bool gsm = (device->technology_family() == shill::kTechnologyFamilyGsm);
  int index = 0;
  for (base::ListValue::const_iterator it = carriers->begin();
       it != carriers->end(); ++it, ++index) {
    std::string value;
    if (!(*it)->GetAsString(&value))
      continue;
    // For GSM devices the device name will be empty, so simply select
    // the Generic UMTS carrier option if present.
    if (gsm && (value == shill::kCarrierGenericUMTS))
      return index;
    // For other carriers, the service name will match the carrier name.
    if (value == device->carrier())
      return index;
  }
  return -1;
}

void PopulateWimaxDetails(const NetworkState* wimax,
                          const base::DictionaryValue& shill_properties,
                          base::DictionaryValue* dictionary) {
  CopyStringFromDictionary(
      shill_properties, shill::kEapIdentityProperty, kTagIdentity, dictionary);
}

void CreateDictionaryFromCellularApn(const base::DictionaryValue* apn,
                                     base::DictionaryValue* dictionary) {
  CopyStringFromDictionary(*apn, shill::kApnProperty, kTagApn, dictionary);
  CopyStringFromDictionary(
      *apn, shill::kApnNetworkIdProperty, kTagNetworkId, dictionary);
  CopyStringFromDictionary(
      *apn, shill::kApnUsernameProperty, kTagUsername, dictionary);
  CopyStringFromDictionary(
      *apn, shill::kApnPasswordProperty, kTagPassword, dictionary);
  CopyStringFromDictionary(*apn, shill::kApnNameProperty, kTagName, dictionary);
  CopyStringFromDictionary(
      *apn, shill::kApnLocalizedNameProperty, kTagLocalizedName, dictionary);
  CopyStringFromDictionary(
      *apn, shill::kApnLanguageProperty, kTagLanguage, dictionary);
}

void PopulateCellularDetails(const NetworkState* cellular,
                             const base::DictionaryValue& shill_properties,
                             base::DictionaryValue* dictionary) {
  dictionary->SetBoolean(kTagCarrierSelectFlag,
                         CommandLine::ForCurrentProcess()->HasSwitch(
                             chromeos::switches::kEnableCarrierSwitching));
  // Cellular network / connection settings.
  dictionary->SetString(kTagActivationState,
                        internet_options_strings::ActivationStateString(
                            cellular->activation_state()));
  dictionary->SetString(kTagRoamingState,
                        internet_options_strings::RoamingStateString(
                            cellular->roaming()));
  dictionary->SetString(kTagRestrictedPool,
                        internet_options_strings::RestrictedStateString(
                            cellular->connection_state()));

  const base::DictionaryValue* olp = NULL;
  if (shill_properties.GetDictionaryWithoutPathExpansion(
          shill::kPaymentPortalProperty, &olp)) {
    std::string url;
    olp->GetStringWithoutPathExpansion(shill::kPaymentPortalURL, &url);
    dictionary->SetString(kTagSupportUrl, url);
  }

  base::DictionaryValue* apn = new base::DictionaryValue;
  const base::DictionaryValue* source_apn = NULL;
  if (shill_properties.GetDictionaryWithoutPathExpansion(
          shill::kCellularApnProperty, &source_apn)) {
    CreateDictionaryFromCellularApn(source_apn, apn);
  }
  dictionary->Set(kTagApn, apn);

  base::DictionaryValue* last_good_apn = new base::DictionaryValue;
  if (shill_properties.GetDictionaryWithoutPathExpansion(
          shill::kCellularLastGoodApnProperty, &source_apn)) {
    CreateDictionaryFromCellularApn(source_apn, last_good_apn);
  }
  dictionary->Set(kTagLastGoodApn, last_good_apn);

  // These default to empty and are only set if device != NULL.
  std::string carrier_id;
  std::string mdn;

  // Device settings.
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          cellular->device_path());
  if (device) {
    const base::DictionaryValue& device_properties = device->properties();
    ::onc::ONCSource onc_source;
    NetworkHandler::Get()->managed_network_configuration_handler()->
        FindPolicyByGUID(LoginState::Get()->primary_user_hash(),
                         cellular->guid(), &onc_source);
    const NetworkPropertyUIData cellular_property_ui_data(onc_source);
    SetValueDictionary(dictionary,
                       kTagSimCardLockEnabled,
                       new base::FundamentalValue(device->sim_lock_enabled()),
                       cellular_property_ui_data);

    carrier_id = device->home_provider_id();
    device_properties.GetStringWithoutPathExpansion(shill::kMdnProperty, &mdn);

    MobileConfig* config = MobileConfig::GetInstance();
    if (config->IsReady()) {
      const MobileConfig::Carrier* carrier = config->GetCarrier(carrier_id);
      if (carrier && !carrier->top_up_url().empty())
        dictionary->SetString(kTagCarrierUrl, carrier->top_up_url());
    }

    base::ListValue* apn_list_value = new base::ListValue();
    const base::ListValue* apn_list;
    if (device_properties.GetListWithoutPathExpansion(
            shill::kCellularApnListProperty, &apn_list)) {
      for (base::ListValue::const_iterator iter = apn_list->begin();
           iter != apn_list->end();
           ++iter) {
        const base::DictionaryValue* dict;
        if ((*iter)->GetAsDictionary(&dict)) {
          base::DictionaryValue* apn = new base::DictionaryValue;
          CreateDictionaryFromCellularApn(dict, apn);
          apn_list_value->Append(apn);
        }
      }
    }
    SetValueDictionary(dictionary,
                       kTagProviderApnList,
                       apn_list_value,
                       cellular_property_ui_data);
    const base::ListValue* supported_carriers;
    if (device_properties.GetListWithoutPathExpansion(
            shill::kSupportedCarriersProperty, &supported_carriers)) {
      dictionary->Set(kTagCarriers, supported_carriers->DeepCopy());
      dictionary->SetInteger(
          kTagCurrentCarrierIndex,
          FindCurrentCarrierIndex(supported_carriers, device));
    } else {
      // In case of any error, set the current carrier tag to -1 indicating
      // to the JS code to fallback to a single carrier.
      dictionary->SetInteger(kTagCurrentCarrierIndex, -1);
    }
  }

  // Don't show any account management related buttons if the activation
  // state is unknown or no payment portal URL is available.
  std::string support_url;
  if (cellular->activation_state() == shill::kActivationStateUnknown ||
      !dictionary->GetString(kTagSupportUrl, &support_url) ||
      support_url.empty()) {
    VLOG(2) << "No support URL is available. Don't display buttons.";
    return;
  }

  if (cellular->activation_state() != shill::kActivationStateActivating &&
      cellular->activation_state() != shill::kActivationStateActivated) {
    dictionary->SetBoolean(kTagShowActivateButton, true);
  } else {
    bool may_show_portal_button = false;

    // If an online payment URL was provided by shill, then this means that the
    // "View Account" button should be shown for the current carrier.
    if (olp) {
      std::string url;
      olp->GetStringWithoutPathExpansion(shill::kPaymentPortalURL, &url);
      may_show_portal_button = !url.empty();
    }
    // If no online payment URL was provided by shill, fall back to
    // MobileConfig to determine if the "View Account" should be shown.
    if (!may_show_portal_button && MobileConfig::GetInstance()->IsReady()) {
      const MobileConfig::Carrier* carrier =
          MobileConfig::GetInstance()->GetCarrier(carrier_id);
      may_show_portal_button = carrier && carrier->show_portal_button();
    }
    if (may_show_portal_button) {
      // The button should be shown for a LTE network even when the LTE network
      // is not connected, but CrOS is online. This is done to enable users to
      // update their plan even if they are out of credits.
      // The button should not be shown when the device's mdn is not set,
      // because the network's proper portal url cannot be generated without it
      const NetworkState* default_network =
          NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
      const std::string& technology = cellular->network_technology();
      bool force_show_view_account_button =
          (technology == shill::kNetworkTechnologyLte ||
           technology == shill::kNetworkTechnologyLteAdvanced) &&
          default_network && !mdn.empty();

      // The button will trigger ShowMorePlanInfoCallback() which will open
      // carrier specific portal.
      if (cellular->IsConnectedState() || force_show_view_account_button)
        dictionary->SetBoolean(kTagShowViewAccountButton, true);
    }
  }
}

scoped_ptr<base::DictionaryValue> PopulateConnectionDetails(
    const NetworkState* network,
    const base::DictionaryValue& shill_properties) {
  // TODO(stevenjb): Once we eliminate all references to Shill properties,
  // we can switch to using managed_network_configuration_handler which
  // includes Device properties for Cellular (and skip the call to
  // onc::TranslateShillServiceToONCPart). For now we copy them over here.
  scoped_ptr<base::DictionaryValue> shill_properties_with_device(
      shill_properties.DeepCopy());
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          network->device_path());
  if (device) {
    shill_properties_with_device->Set(shill::kDeviceProperty,
                                      device->properties().DeepCopy());
  }
  scoped_ptr<base::DictionaryValue> dictionary =
      onc::TranslateShillServiceToONCPart(
          *shill_properties_with_device, &onc::kNetworkWithStateSignature);

  dictionary->SetString(kNetworkInfoKeyServicePath, network->path());
  dictionary->SetString(
      kTagErrorState,
      ash::network_connect::ErrorString(network->error(), network->path()));

  dictionary->SetBoolean(kTagRemembered, !network->profile_path().empty());
  bool shared = !network->IsPrivate();
  dictionary->SetBoolean(kTagShared, shared);

  const std::string& type = network->type();

  const NetworkState* connected_network =
      NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::Primitive(type));
  dictionary->SetBoolean(kTagDeviceConnected, connected_network != NULL);

  if (type == shill::kTypeWimax)
    PopulateWimaxDetails(network, shill_properties, dictionary.get());
  else if (type == shill::kTypeCellular)
    PopulateCellularDetails(network, shill_properties, dictionary.get());
  else if (type == shill::kTypeVPN)
    PopulateVPNDetails(network, shill_properties, dictionary.get());

  return dictionary.Pass();
}

// Helper methods for SetIPConfigProperties
bool AppendPropertyKeyIfPresent(const std::string& key,
                                const base::DictionaryValue& old_properties,
                                std::vector<std::string>* property_keys) {
  if (old_properties.HasKey(key)) {
    property_keys->push_back(key);
    return true;
  }
  return false;
}

bool AddStringPropertyIfChanged(const std::string& key,
                                const std::string& new_value,
                                const base::DictionaryValue& old_properties,
                                base::DictionaryValue* new_properties) {
  std::string old_value;
  if (!old_properties.GetStringWithoutPathExpansion(key, &old_value) ||
      new_value != old_value) {
    new_properties->SetStringWithoutPathExpansion(key, new_value);
    return true;
  }
  return false;
}

bool AddIntegerPropertyIfChanged(const std::string& key,
                                 int new_value,
                                 const base::DictionaryValue& old_properties,
                                 base::DictionaryValue* new_properties) {
  int old_value;
  if (!old_properties.GetIntegerWithoutPathExpansion(key, &old_value) ||
      new_value != old_value) {
    new_properties->SetIntegerWithoutPathExpansion(key, new_value);
    return true;
  }
  return false;
}

void RequestReconnect(const std::string& service_path,
                      gfx::NativeWindow owning_window) {
  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      service_path,
      base::Bind(&ash::network_connect::ConnectToNetwork,
                 service_path, owning_window),
      base::Bind(&ShillError, "RequestReconnect"));
}

}  // namespace

InternetOptionsHandler::InternetOptionsHandler()
    : weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_ENTER_PIN_ENDED,
                 content::NotificationService::AllSources());
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

InternetOptionsHandler::~InternetOptionsHandler() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

void InternetOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  internet_options_strings::RegisterLocalizedStrings(localized_strings);

  // TODO(stevenjb): Find a better way to populate initial data before
  // InitializePage() gets called.
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
  localized_strings->SetString("ownerUserId", base::UTF8ToUTF16(owner));
  bool logged_in_as_owner = LoginState::Get()->GetLoggedInUserType() ==
                            LoginState::LOGGED_IN_USER_OWNER;
  localized_strings->SetBoolean("loggedInAsOwner", logged_in_as_owner);

  base::DictionaryValue* network_dictionary = new base::DictionaryValue;
  FillNetworkInfo(network_dictionary);
  localized_strings->Set("networkData", network_dictionary);
}

void InternetOptionsHandler::InitializePage() {
  base::DictionaryValue dictionary;
  dictionary.SetString(::onc::network_type::kCellular,
      GetIconDataUrl(IDR_AURA_UBER_TRAY_NETWORK_BARS_DARK));
  dictionary.SetString(::onc::network_type::kWiFi,
      GetIconDataUrl(IDR_AURA_UBER_TRAY_NETWORK_ARCS_DARK));
  dictionary.SetString(::onc::network_type::kVPN,
      GetIconDataUrl(IDR_AURA_UBER_TRAY_NETWORK_VPN));
  web_ui()->CallJavascriptFunction(kSetDefaultNetworkIconsFunction,
                                   dictionary);
  NetworkHandler::Get()->network_state_handler()->RequestScan();
  RefreshNetworkData();
}

void InternetOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback(kNetworkCommandMessage,
      base::Bind(&InternetOptionsHandler::NetworkCommandCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kRefreshNetworksMessage,
      base::Bind(&InternetOptionsHandler::RefreshNetworksCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetPreferNetworkMessage,
      base::Bind(&InternetOptionsHandler::SetPreferNetworkCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetAutoConnectMessage,
      base::Bind(&InternetOptionsHandler::SetAutoConnectCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetIPConfigMessage,
      base::Bind(&InternetOptionsHandler::SetIPConfigCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kEnableWifiMessage,
      base::Bind(&InternetOptionsHandler::EnableWifiCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDisableWifiMessage,
      base::Bind(&InternetOptionsHandler::DisableWifiCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kEnableCellularMessage,
      base::Bind(&InternetOptionsHandler::EnableCellularCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDisableCellularMessage,
      base::Bind(&InternetOptionsHandler::DisableCellularCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kEnableWimaxMessage,
      base::Bind(&InternetOptionsHandler::EnableWimaxCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDisableWimaxMessage,
      base::Bind(&InternetOptionsHandler::DisableWimaxCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kBuyDataPlanMessage,
      base::Bind(&InternetOptionsHandler::BuyDataPlanCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kShowMorePlanInfoMessage,
      base::Bind(&InternetOptionsHandler::ShowMorePlanInfoCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetApnMessage,
      base::Bind(&InternetOptionsHandler::SetApnCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetCarrierMessage,
      base::Bind(&InternetOptionsHandler::SetCarrierCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetSimCardLockMessage,
      base::Bind(&InternetOptionsHandler::SetSimCardLockCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kChangePinMessage,
      base::Bind(&InternetOptionsHandler::ChangePinCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetServerHostname,
      base::Bind(&InternetOptionsHandler::SetServerHostnameCallback,
                 base::Unretained(this)));
}

void InternetOptionsHandler::EnableWifiCallback(const base::ListValue* args) {
  content::RecordAction(base::UserMetricsAction("Options_NetworkWifiToggle"));
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), true,
      base::Bind(&ShillError, "EnableWifiCallback"));
}

void InternetOptionsHandler::DisableWifiCallback(const base::ListValue* args) {
  content::RecordAction(base::UserMetricsAction("Options_NetworkWifiToggle"));
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false,
      base::Bind(&ShillError, "DisableWifiCallback"));
}

void InternetOptionsHandler::EnableCellularCallback(
    const base::ListValue* args) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const DeviceState* device =
      handler->GetDeviceStateByType(NetworkTypePattern::Cellular());
  if (!device) {
    LOG(ERROR) << "Mobile device not found.";
    return;
  }
  if (!device->sim_lock_type().empty()) {
    SimDialogDelegate::ShowDialog(GetNativeWindow(),
                                  SimDialogDelegate::SIM_DIALOG_UNLOCK);
    return;
  }
  if (!handler->IsTechnologyEnabled(NetworkTypePattern::Cellular())) {
    handler->SetTechnologyEnabled(
        NetworkTypePattern::Cellular(), true,
        base::Bind(&ShillError, "EnableCellularCallback"));
    return;
  }
  if (device->IsSimAbsent()) {
    mobile_config_ui::DisplayConfigDialog();
    return;
  }
  LOG(ERROR) << "EnableCellularCallback called for enabled mobile device";
}

void InternetOptionsHandler::DisableCellularCallback(
    const base::ListValue* args) {
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Mobile(), false,
      base::Bind(&ShillError, "DisableCellularCallback"));
}

void InternetOptionsHandler::EnableWimaxCallback(const base::ListValue* args) {
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Wimax(), true,
      base::Bind(&ShillError, "EnableWimaxCallback"));
}

void InternetOptionsHandler::DisableWimaxCallback(const base::ListValue* args) {
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Wimax(), false,
      base::Bind(&ShillError, "DisableWimaxCallback"));
}

void InternetOptionsHandler::ShowMorePlanInfoCallback(
    const base::ListValue* args) {
  if (!web_ui())
    return;
  std::string service_path;
  if (args->GetSize() != 1 || !args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  ash::network_connect::ShowMobileSetup(service_path);
}

void InternetOptionsHandler::BuyDataPlanCallback(const base::ListValue* args) {
  if (!web_ui())
    return;
  std::string service_path;
  if (args->GetSize() != 1 || !args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  ash::network_connect::ShowMobileSetup(service_path);
}

void InternetOptionsHandler::SetApnCallback(const base::ListValue* args) {
  std::string service_path;
  if (!args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      service_path,
      base::Bind(&InternetOptionsHandler::SetApnProperties,
                 weak_factory_.GetWeakPtr(), base::Owned(args->DeepCopy())),
      base::Bind(&ShillError, "SetApnCallback"));
}

void InternetOptionsHandler::SetApnProperties(
    const base::ListValue* args,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  std::string apn, username, password;
  if (!args->GetString(1, &apn) ||
      !args->GetString(2, &username) ||
      !args->GetString(3, &password)) {
    NOTREACHED();
    return;
  }
  NET_LOG_EVENT("SetApnCallback", service_path);

  if (apn.empty()) {
    std::vector<std::string> properties_to_clear;
    properties_to_clear.push_back(shill::kCellularApnProperty);
    NetworkHandler::Get()->network_configuration_handler()->ClearProperties(
      service_path, properties_to_clear,
      base::Bind(&base::DoNothing),
      base::Bind(&ShillError, "ClearCellularApnProperties"));
    return;
  }

  const base::DictionaryValue* shill_apn_dict = NULL;
  std::string network_id;
  if (shill_properties.GetDictionaryWithoutPathExpansion(
          shill::kCellularApnProperty, &shill_apn_dict)) {
    shill_apn_dict->GetStringWithoutPathExpansion(
        shill::kApnNetworkIdProperty, &network_id);
  }
  base::DictionaryValue properties;
  base::DictionaryValue* apn_dict = new base::DictionaryValue;
  apn_dict->SetStringWithoutPathExpansion(shill::kApnProperty, apn);
  apn_dict->SetStringWithoutPathExpansion(shill::kApnNetworkIdProperty,
                                          network_id);
  apn_dict->SetStringWithoutPathExpansion(shill::kApnUsernameProperty,
                                          username);
  apn_dict->SetStringWithoutPathExpansion(shill::kApnPasswordProperty,
                                          password);
  properties.SetWithoutPathExpansion(shill::kCellularApnProperty, apn_dict);
  NetworkHandler::Get()->network_configuration_handler()->SetProperties(
      service_path, properties,
      base::Bind(&base::DoNothing),
      base::Bind(&ShillError, "SetApnProperties"));
}

void InternetOptionsHandler::CarrierStatusCallback() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const DeviceState* device =
      handler->GetDeviceStateByType(NetworkTypePattern::Cellular());
  if (device && (device->carrier() == shill::kCarrierSprint)) {
    const NetworkState* network =
        handler->FirstNetworkByType(NetworkTypePattern::Cellular());
    if (network) {
      ash::network_connect::ActivateCellular(network->path());
      UpdateConnectionData(network->path());
    }
  }
  UpdateCarrier();
}

void InternetOptionsHandler::SetCarrierCallback(const base::ListValue* args) {
  std::string service_path;
  std::string carrier;
  if (args->GetSize() != 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &carrier)) {
    NOTREACHED();
    return;
  }
  const DeviceState* device = NetworkHandler::Get()->network_state_handler()->
      GetDeviceStateByType(NetworkTypePattern::Cellular());
  if (!device) {
    LOG(WARNING) << "SetCarrierCallback with no cellular device.";
    return;
  }
  NetworkHandler::Get()->network_device_handler()->SetCarrier(
      device->path(),
      carrier,
      base::Bind(&InternetOptionsHandler::CarrierStatusCallback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ShillError, "SetCarrierCallback"));
}

void InternetOptionsHandler::SetSimCardLockCallback(
    const base::ListValue* args) {
  bool require_pin_new_value;
  if (!args->GetBoolean(0, &require_pin_new_value)) {
    NOTREACHED();
    return;
  }
  // 1. Bring up SIM unlock dialog, pass new RequirePin setting in URL.
  // 2. Dialog will ask for current PIN in any case.
  // 3. If card is locked it will first call PIN unlock operation
  // 4. Then it will call Set RequirePin, passing the same PIN.
  // 5. We'll get notified by REQUIRE_PIN_SETTING_CHANGE_ENDED notification.
  SimDialogDelegate::SimDialogMode mode;
  if (require_pin_new_value)
    mode = SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON;
  else
    mode = SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF;
  SimDialogDelegate::ShowDialog(GetNativeWindow(), mode);
}

void InternetOptionsHandler::ChangePinCallback(const base::ListValue* args) {
  SimDialogDelegate::ShowDialog(GetNativeWindow(),
                                SimDialogDelegate::SIM_DIALOG_CHANGE_PIN);
}

void InternetOptionsHandler::RefreshNetworksCallback(
    const base::ListValue* args) {
  NetworkHandler::Get()->network_state_handler()->RequestScan();
}

std::string InternetOptionsHandler::GetIconDataUrl(int resource_id) const {
  gfx::ImageSkia* icon =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  gfx::ImageSkiaRep image_rep = icon->GetRepresentation(
      web_ui()->GetDeviceScaleFactor());
  return webui::GetBitmapDataUrl(image_rep.sk_bitmap());
}

void InternetOptionsHandler::RefreshNetworkData() {
  base::DictionaryValue dictionary;
  FillNetworkInfo(&dictionary);
  web_ui()->CallJavascriptFunction(kRefreshNetworkDataFunction, dictionary);
}

void InternetOptionsHandler::UpdateConnectionData(
    const std::string& service_path) {
  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      service_path,
      base::Bind(&InternetOptionsHandler::UpdateConnectionDataCallback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ShillError, "UpdateConnectionData"));
}

void InternetOptionsHandler::UpdateConnectionDataCallback(
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network)
    return;
  scoped_ptr<base::DictionaryValue> dictionary =
      PopulateConnectionDetails(network, shill_properties);
  web_ui()->CallJavascriptFunction(kUpdateConnectionDataFunction, *dictionary);
}

void InternetOptionsHandler::UpdateCarrier() {
  web_ui()->CallJavascriptFunction(kUpdateCarrierFunction);
}

void InternetOptionsHandler::DeviceListChanged() {
  if (!web_ui())
    return;
  RefreshNetworkData();
}

void InternetOptionsHandler::NetworkListChanged() {
  if (!web_ui())
    return;
  RefreshNetworkData();
}

void InternetOptionsHandler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!web_ui())
    return;
  // Update the connection data for the detailed view when the connection state
  // of any network changes.
  if (!details_path_.empty())
    UpdateConnectionData(details_path_);
}

void InternetOptionsHandler::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (!web_ui())
    return;
  RefreshNetworkData();
  UpdateConnectionData(network->path());
}

void InternetOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED) {
    base::FundamentalValue require_pin(*content::Details<bool>(details).ptr());
    web_ui()->CallJavascriptFunction(
        kUpdateSecurityTabFunction, require_pin);
  } else if (type == chrome::NOTIFICATION_ENTER_PIN_ENDED) {
    // We make an assumption (which is valid for now) that the SIM
    // unlock dialog is put up only when the user is trying to enable
    // mobile data.
    bool cancelled = *content::Details<bool>(details).ptr();
    if (cancelled)
      RefreshNetworkData();
    // The case in which the correct PIN was entered and the SIM is
    // now unlocked is handled in NetworkMenuButton.
  }
}

void InternetOptionsHandler::SetServerHostnameCallback(
    const base::ListValue* args) {
  std::string service_path, server_hostname;
  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &server_hostname)) {
    NOTREACHED();
    return;
  }
  SetNetworkProperty(service_path,
                     shill::kProviderHostProperty,
                     new base::StringValue(server_hostname));
}

void InternetOptionsHandler::SetPreferNetworkCallback(
    const base::ListValue* args) {
  std::string service_path, prefer_network_str;
  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &prefer_network_str)) {
    NOTREACHED();
    return;
  }
  content::RecordAction(base::UserMetricsAction("Options_NetworkSetPrefer"));
  int priority = (prefer_network_str == kTagTrue) ? kPreferredPriority : 0;
  SetNetworkProperty(service_path,
                     shill::kPriorityProperty,
                     new base::FundamentalValue(priority));
}

void InternetOptionsHandler::SetAutoConnectCallback(
    const base::ListValue* args) {
  std::string service_path, auto_connect_str;
  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &auto_connect_str)) {
    NOTREACHED();
    return;
  }
  content::RecordAction(base::UserMetricsAction("Options_NetworkAutoConnect"));
  bool auto_connect = auto_connect_str == kTagTrue;
  SetNetworkProperty(service_path,
                     shill::kAutoConnectProperty,
                     new base::FundamentalValue(auto_connect));
}

void InternetOptionsHandler::SetIPConfigCallback(const base::ListValue* args) {
  std::string service_path;
  if (!args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      service_path,
      base::Bind(&InternetOptionsHandler::SetIPConfigProperties,
                 weak_factory_.GetWeakPtr(), base::Owned(args->DeepCopy())),
      base::Bind(&ShillError, "SetIPConfigCallback"));
}

void InternetOptionsHandler::SetIPConfigProperties(
    const base::ListValue* args,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  std::string address, netmask, gateway, name_server_type, name_servers;
  bool dhcp_for_ip;
  if (!args->GetBoolean(1, &dhcp_for_ip) ||
      !args->GetString(2, &address) ||
      !args->GetString(3, &netmask) ||
      !args->GetString(4, &gateway) ||
      !args->GetString(5, &name_server_type) ||
      !args->GetString(6, &name_servers)) {
    NOTREACHED();
    return;
  }
  NET_LOG_USER("SetIPConfigProperties", service_path);

  bool request_reconnect = false;
  std::vector<std::string> properties_to_clear;
  base::DictionaryValue properties_to_set;

  if (dhcp_for_ip) {
    request_reconnect |= AppendPropertyKeyIfPresent(
        shill::kStaticIPAddressProperty,
        shill_properties, &properties_to_clear);
    request_reconnect |= AppendPropertyKeyIfPresent(
        shill::kStaticIPPrefixlenProperty,
        shill_properties, &properties_to_clear);
    request_reconnect |= AppendPropertyKeyIfPresent(
        shill::kStaticIPGatewayProperty,
        shill_properties, &properties_to_clear);
  } else {
    request_reconnect |= AddStringPropertyIfChanged(
        shill::kStaticIPAddressProperty,
        address, shill_properties, &properties_to_set);
    int prefixlen = network_util::NetmaskToPrefixLength(netmask);
    if (prefixlen < 0) {
      LOG(ERROR) << "Invalid prefix length for: " << service_path
                 << " with netmask " << netmask;
      prefixlen = 0;
    }
    request_reconnect |= AddIntegerPropertyIfChanged(
        shill::kStaticIPPrefixlenProperty,
        prefixlen, shill_properties, &properties_to_set);
    request_reconnect |= AddStringPropertyIfChanged(
        shill::kStaticIPGatewayProperty,
        gateway, shill_properties, &properties_to_set);
  }

  if (name_server_type == kNameServerTypeAutomatic) {
    AppendPropertyKeyIfPresent(shill::kStaticIPNameServersProperty,
                               shill_properties, &properties_to_clear);
  } else {
    if (name_server_type == kNameServerTypeGoogle)
      name_servers = kGoogleNameServers;
    AddStringPropertyIfChanged(
        shill::kStaticIPNameServersProperty,
        name_servers, shill_properties, &properties_to_set);
  }

  if (!properties_to_clear.empty()) {
    NetworkHandler::Get()->network_configuration_handler()->ClearProperties(
      service_path, properties_to_clear,
      base::Bind(&base::DoNothing),
      base::Bind(&ShillError, "ClearIPConfigProperties"));
  }
  if (!properties_to_set.empty()) {
    NetworkHandler::Get()->network_configuration_handler()->SetProperties(
        service_path, properties_to_set,
        base::Bind(&base::DoNothing),
        base::Bind(&ShillError, "SetIPConfigProperties"));
  }
  std::string device_path;
  shill_properties.GetStringWithoutPathExpansion(
      shill::kDeviceProperty, &device_path);
  if (!device_path.empty()) {
    base::Closure callback = base::Bind(&base::DoNothing);
    // If auto config or a static IP property changed, we need to reconnect
    // to the network.
    if (request_reconnect)
      callback = base::Bind(&RequestReconnect, service_path, GetNativeWindow());
    NetworkHandler::Get()->network_device_handler()->RequestRefreshIPConfigs(
        device_path,
        callback,
        base::Bind(&ShillError, "RequestRefreshIPConfigs"));
  }
}

void InternetOptionsHandler::PopulateDictionaryDetailsCallback(
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    LOG(ERROR) << "Network properties not found: " << service_path;
    return;
  }

  details_path_ = service_path;

  scoped_ptr<base::DictionaryValue> dictionary =
      PopulateConnectionDetails(network, shill_properties);

  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  const base::DictionaryValue* onc =
      onc::FindPolicyForActiveUser(network->guid(), &onc_source);
  const NetworkPropertyUIData property_ui_data(onc_source);

  // IP config
  scoped_ptr<base::DictionaryValue> ipconfig_dhcp(new base::DictionaryValue);
  ipconfig_dhcp->SetString(kIpConfigAddress, network->ip_address());
  ipconfig_dhcp->SetString(kIpConfigNetmask, network->GetNetmask());
  ipconfig_dhcp->SetString(kIpConfigGateway, network->gateway());
  std::string ipconfig_name_servers = network->GetDnsServersAsString();
  ipconfig_dhcp->SetString(kIpConfigNameServers, ipconfig_name_servers);
  ipconfig_dhcp->SetString(kIpConfigWebProxyAutoDiscoveryUrl,
                           network->web_proxy_auto_discovery_url().spec());
  SetValueDictionary(dictionary.get(),
                     kDictionaryIpConfig,
                     ipconfig_dhcp.release(),
                     property_ui_data);

  std::string name_server_type = kNameServerTypeAutomatic;
  int automatic_ip_config = 0;
  scoped_ptr<base::DictionaryValue> static_ip_dict(
      BuildIPInfoDictionary(shill_properties, true, &automatic_ip_config));
  dictionary->SetBoolean(kIpConfigAutoConfig, automatic_ip_config == 0);
  DCHECK(automatic_ip_config == 3 || automatic_ip_config == 0)
      << "UI doesn't support automatic specification of individual "
      << "static IP parameters.";
  scoped_ptr<base::DictionaryValue> saved_ip_dict(
      BuildIPInfoDictionary(shill_properties, false, NULL));
  dictionary->Set(kDictionarySavedIp, saved_ip_dict.release());

  // Determine what kind of name server setting we have by comparing the
  // StaticIP and Google values with the ipconfig values.
  std::string static_ip_nameservers;
  static_ip_dict->GetString(kIpConfigNameServers, &static_ip_nameservers);
  if (!static_ip_nameservers.empty() &&
      static_ip_nameservers == ipconfig_name_servers) {
    name_server_type = kNameServerTypeUser;
  }
  if (ipconfig_name_servers == kGoogleNameServers) {
    name_server_type = kNameServerTypeGoogle;
  }
  SetValueDictionary(dictionary.get(),
                     kDictionaryStaticIp,
                     static_ip_dict.release(),
                     property_ui_data);

  dictionary->SetString(kTagNameServerType, name_server_type);
  dictionary->SetString(kTagNameServersGoogle, kGoogleNameServers);

  // Enable static ip config for Ethernet or WiFi.
  bool staticIPConfig = network->Matches(NetworkTypePattern::Ethernet()) ||
                        network->Matches(NetworkTypePattern::WiFi());
  dictionary->SetBoolean(kTagShowStaticIPConfig, staticIPConfig);

  int priority = 0;
  shill_properties.GetIntegerWithoutPathExpansion(
      shill::kPriorityProperty, &priority);
  bool preferred = priority > 0;
  SetValueDictionary(dictionary.get(), kTagPreferred,
                     new base::FundamentalValue(preferred),
                     property_ui_data);

  NetworkPropertyUIData auto_connect_ui_data(onc_source);
  std::string onc_path_to_auto_connect;
  if (network->Matches(NetworkTypePattern::WiFi())) {
    content::RecordAction(
        base::UserMetricsAction("Options_NetworkShowDetailsWifi"));
    if (network->IsConnectedState()) {
      content::RecordAction(
          base::UserMetricsAction("Options_NetworkShowDetailsWifiConnected"));
    }
    onc_path_to_auto_connect =
        ::onc::network_config::WifiProperty(::onc::wifi::kAutoConnect);
  } else if (network->Matches(NetworkTypePattern::VPN())) {
    content::RecordAction(
        base::UserMetricsAction("Options_NetworkShowDetailsVPN"));
    if (network->IsConnectedState()) {
      content::RecordAction(
          base::UserMetricsAction("Options_NetworkShowDetailsVPNConnected"));
    }
    onc_path_to_auto_connect = ::onc::network_config::VpnProperty(
        ::onc::vpn::kAutoConnect);
  } else if (network->Matches(NetworkTypePattern::Cellular())) {
    content::RecordAction(
        base::UserMetricsAction("Options_NetworkShowDetailsCellular"));
    if (network->IsConnectedState()) {
      content::RecordAction(base::UserMetricsAction(
          "Options_NetworkShowDetailsCellularConnected"));
    }
  }
  if (!onc_path_to_auto_connect.empty()) {
    auto_connect_ui_data.ParseOncProperty(
        onc_source, onc, onc_path_to_auto_connect);
  }
  bool auto_connect = false;
  shill_properties.GetBooleanWithoutPathExpansion(
      shill::kAutoConnectProperty, &auto_connect);
  SetAutoconnectValueDictionary(network->IsPrivate(),
                                onc_source,
                                auto_connect,
                                auto_connect_ui_data,
                                dictionary.get());

  // Show details dialog
  web_ui()->CallJavascriptFunction(kShowDetailedInfoFunction, *dictionary);
}

gfx::NativeWindow InternetOptionsHandler::GetNativeWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

float InternetOptionsHandler::GetScaleFactor() const {
  return web_ui()->GetDeviceScaleFactor();
}

const PrefService* InternetOptionsHandler::GetPrefs() const {
  return Profile::FromWebUI(web_ui())->GetPrefs();
}

void InternetOptionsHandler::NetworkCommandCallback(
    const base::ListValue* args) {
  std::string onc_type;
  std::string service_path;
  std::string command;
  if (args->GetSize() != 3 ||
      !args->GetString(0, &onc_type) ||
      !args->GetString(1, &service_path) ||
      !args->GetString(2, &command)) {
    NOTREACHED();
    return;
  }
  std::string type;  // Shill type
  if (!onc_type.empty()) {
    type = network_util::TranslateONCTypeToShill(onc_type);
    if (type.empty())
      LOG(ERROR) << "Unable to translate ONC type: " << onc_type;
  }
  // Process commands that do not require an existing network.
  if (command == kTagAddConnection) {
    AddConnection(type);
  } else if (command == kTagForget) {
    NetworkHandler::Get()->network_configuration_handler()->
        RemoveConfiguration(
            service_path,
            base::Bind(&base::DoNothing),
            base::Bind(&ShillError, "NetworkCommand: " + command));
  } else if (command == kTagOptions) {
    NetworkHandler::Get()->network_configuration_handler()->GetProperties(
        service_path,
        base::Bind(&InternetOptionsHandler::PopulateDictionaryDetailsCallback,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&ShillError, "NetworkCommand: " + command));
  } else if (command == kTagConnect) {
    const NetworkState* network = GetNetworkState(service_path);
    if (network && network->type() == shill::kTypeWifi)
      content::RecordAction(
          base::UserMetricsAction("Options_NetworkConnectToWifi"));
    else if (network && network->type() == shill::kTypeVPN)
      content::RecordAction(
          base::UserMetricsAction("Options_NetworkConnectToVPN"));
    ash::network_connect::ConnectToNetwork(service_path, GetNativeWindow());
  } else if (command == kTagDisconnect) {
    const NetworkState* network = GetNetworkState(service_path);
    if (network && network->type() == shill::kTypeWifi)
      content::RecordAction(
          base::UserMetricsAction("Options_NetworkDisconnectWifi"));
    else if (network && network->type() == shill::kTypeVPN)
      content::RecordAction(
          base::UserMetricsAction("Options_NetworkDisconnectVPN"));
    NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
        service_path,
        base::Bind(&base::DoNothing),
        base::Bind(&ShillError, "NetworkCommand: " + command));
  } else if (command == kTagConfigure) {
    NetworkConfigView::Show(service_path, GetNativeWindow());
  } else if (command == kTagActivate && type == shill::kTypeCellular) {
    ash::network_connect::ActivateCellular(service_path);
    // Activation may update network properties (e.g. ActivationState), so
    // request them here in case they change.
    UpdateConnectionData(service_path);
  } else {
    VLOG(1) << "Unknown command: " << command;
    NOTREACHED();
  }
}

void InternetOptionsHandler::AddConnection(const std::string& type) {
  if (type == shill::kTypeWifi) {
    content::RecordAction(
        base::UserMetricsAction("Options_NetworkJoinOtherWifi"));
    NetworkConfigView::ShowForType(shill::kTypeWifi, GetNativeWindow());
  } else if (type == shill::kTypeVPN) {
    content::RecordAction(
        base::UserMetricsAction("Options_NetworkJoinOtherVPN"));
    NetworkConfigView::ShowForType(shill::kTypeVPN, GetNativeWindow());
  } else if (type == shill::kTypeCellular) {
    ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
  } else {
    LOG(ERROR) << "Unsupported type for AddConnection";
  }
}

base::ListValue* InternetOptionsHandler::GetWiredList() {
  base::ListValue* list = new base::ListValue();
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      FirstNetworkByType(NetworkTypePattern::Ethernet());
  if (!network)
    return list;
  list->Append(BuildNetworkDictionary(network, GetScaleFactor(), GetPrefs()));
  return list;
}

base::ListValue* InternetOptionsHandler::GetWirelessList() {
  base::ListValue* list = new base::ListValue();

  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkListByType(
      NetworkTypePattern::Wireless(), &networks);
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin(); iter != networks.end(); ++iter) {
    list->Append(BuildNetworkDictionary(*iter, GetScaleFactor(), GetPrefs()));
  }

  return list;
}

base::ListValue* InternetOptionsHandler::GetVPNList() {
  base::ListValue* list = new base::ListValue();

  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkListByType(
      NetworkTypePattern::VPN(), &networks);
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin(); iter != networks.end(); ++iter) {
    list->Append(BuildNetworkDictionary(*iter, GetScaleFactor(), GetPrefs()));
  }

  return list;
}

base::ListValue* InternetOptionsHandler::GetRememberedList() {
  base::ListValue* list = new base::ListValue();

  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true /* configured_only */,
      false /* visible_only */,
      0 /* no limit */,
      &networks);
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin(); iter != networks.end(); ++iter) {
    const NetworkState* network = *iter;
    if (network->type() != shill::kTypeWifi &&
        network->type() != shill::kTypeVPN)
      continue;
    list->Append(
        BuildNetworkDictionary(network,
                               web_ui()->GetDeviceScaleFactor(),
                               Profile::FromWebUI(web_ui())->GetPrefs()));
  }

  return list;
}

void InternetOptionsHandler::FillNetworkInfo(
    base::DictionaryValue* dictionary) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  dictionary->Set(kTagWiredList, GetWiredList());
  dictionary->Set(kTagWirelessList, GetWirelessList());
  dictionary->Set(kTagVpnList, GetVPNList());
  dictionary->Set(kTagRememberedList, GetRememberedList());

  dictionary->SetBoolean(
      kTagWifiAvailable,
      handler->IsTechnologyAvailable(NetworkTypePattern::WiFi()));
  dictionary->SetBoolean(
      kTagWifiEnabled,
      handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  dictionary->SetBoolean(
      kTagCellularAvailable,
      handler->IsTechnologyAvailable(NetworkTypePattern::Mobile()));
  dictionary->SetBoolean(
      kTagCellularEnabled,
      handler->IsTechnologyEnabled(NetworkTypePattern::Mobile()));
  const DeviceState* cellular =
      handler->GetDeviceStateByType(NetworkTypePattern::Mobile());
  dictionary->SetBoolean(
      kTagCellularSupportsScan,
      cellular && cellular->support_network_scan());

  dictionary->SetBoolean(
      kTagWimaxAvailable,
      handler->IsTechnologyAvailable(NetworkTypePattern::Wimax()));
  dictionary->SetBoolean(
      kTagWimaxEnabled,
      handler->IsTechnologyEnabled(NetworkTypePattern::Wimax()));
}

}  // namespace options
}  // namespace chromeos
