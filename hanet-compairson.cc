
#include "ns3/animation-interface.h"
#include "ns3/command-line.h"
#include "ns3/csma-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/qos-txop.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

//
// Define logging keyword for this file
//
NS_LOG_COMPONENT_DEFINE("MixedWireless");

int
main(int argc, char* argv[])
{
    //
    // First, we declare and initialize a few local variables that control some
    // simulation parameters.
    //

    uint32_t manetNodes = 10;

    //
    // Simulation defaults are typically set next, before command line
    // arguments are parsed.
    //
    
    Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("1472"));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("100kb/s"));
    //
    // Create a container to manage the nodes of the adhoc (manet) network.
    // Later we'll create the rest of the nodes we'll need.
    //
    NodeContainer adhocContainer;
    adhocContainer.Create(manetNodes);
    //
    // Create the manet wifi net devices and install them into the nodes in
    // our container
    //
    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("OfdmRate54Mbps"));
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    NetDeviceContainer manetDevices = wifi.Install(wifiPhy, mac, adhocContainer);
}