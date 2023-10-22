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
#include "ns3/core-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

//
// Define logging keyword for this file
//
NS_LOG_COMPONENT_DEFINE("hanet-compairsonV2");

/**
 * This function will be used below as a trace sink, if the command-line
 * argument or default value "useCourseChangeCallback" is set to true
 *
 * \param path The callback path.
 * \param model The mobility model.
 */
static void
CourseChangeCallback(std::string path, Ptr<const MobilityModel> model)
{
    Vector position = model->GetPosition();
    std::cout << "CourseChange " << path << " x=" << position.x << ", y=" << position.y
              << ", z=" << position.z << std::endl;
}

int
main(int argc, char* argv[])
{
    //
    // First, we declare and initialize a few local variables that control some
    // simulation parameters.
    //
    uint32_t manetNodes = 10;
    uint32_t mobileNodes = 2;
    std::string m_protocolName = "OLSR";
    uint32_t stopTime = 20;
    bool useCourseChangeCallback = false;

    //
    // Simulation defaults are typically set next, before command line
    // arguments are parsed.
    //
    Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("1472"));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("100kb/s"));

    //
    // For convenience, we add the local variables to the command line argument
    // system so that they can be overridden with flags such as
    // "--manetNodes=20"
    //
    CommandLine cmd(__FILE__);
    cmd.AddValue("manetNodes", "number of manet nodes", manetNodes);
    cmd.AddValue("mobileNodes", "number of leaf nodes", mobileNodes);
    cmd.AddValue("protocol", "routing protocol for manet nodes", m_protocolName);
    cmd.AddValue("stopTime", "simulation stop time (seconds)", stopTime);
    cmd.AddValue("useCourseChangeCallback",
                 "whether to enable course change tracing",
                 useCourseChangeCallback);

    //
    // The system global variables and the local values added to the argument
    // system can be overridden by command line arguments by using this call.
    //
    cmd.Parse(argc, argv);

    if (stopTime < 10)
    {
        std::cout << "Use a simulation stop time >= 10 seconds" << std::endl;
        exit(1);
    }
    ///////////////////////////////////////////////////////////////////////////
    //                                                                       //
    // Construct the manet                                                //
    //                                                                       //
    ///////////////////////////////////////////////////////////////////////////

    //
    // Create a container to manage the nodes of the adhoc (manet) network.
    // Later we'll create the rest of the nodes we'll need.
    //
    NodeContainer manet;
    manet.Create(manetNodes);
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
    NetDeviceContainer manetDevices = wifi.Install(wifiPhy, mac, manet);

    // We enable OLSR (which will be consulted at a higher priority than
    // the global routing) on the manet ad hoc nodes
    NS_LOG_INFO("Enabling routing protocols on all manet nodes");
    OlsrHelper olsr;
    AodvHelper aodv;
    DsdvHelper dsdv;
    DsrMainHelper dsrMain;
    Ipv4ListRoutingHelper list;

    //
    //
    // Add the IPv4 protocol stack to the nodes in our container
    //
    InternetStackHelper internet;
    NS_LOG_UNCOND(m_protocolName);
    // change here the parameter to use the routing protocol
    if (m_protocolName == "OLSR"){
        internet.SetRoutingHelper(olsr);
    } else if (m_protocolName == "AODV"){
        internet.SetRoutingHelper(aodv);
    } else if (m_protocolName == "DSDV"){
        internet.SetRoutingHelper(dsdv);
    } else {
        NS_FATAL_ERROR("No routing protocol selected or no such protocol existant");
    }
     // has effect on the next Install ()
    internet.Install(manet);

    //
    // Assign IPv4 addresses to the device drivers (actually to the associated
    // IPv4 interfaces) we just created.
    //
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("192.168.0.0", "255.255.255.0");
    ipAddrs.Assign(manetDevices);

    //
    // The ad-hoc network nodes need a mobility model so we aggregate one to
    // each of the nodes we just finished building.
    //
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(20.0),
                                  "MinY",
                                  DoubleValue(20.0),
                                  "DeltaX",
                                  DoubleValue(20.0),
                                  "DeltaY",
                                  DoubleValue(20.0),
                                  "GridWidth",
                                  UintegerValue(5),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-500, 500, -500, 500)),
                              "Speed",
                              StringValue("ns3::ConstantRandomVariable[Constant=2]"),
                              "Pause",
                              StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    mobility.Install(manet);


    ipAddrs.SetBase("10.0.0.0", "255.255.255.0");

    for (uint32_t i = 0; i < manetNodes; ++i)
    {
        NS_LOG_INFO("Configuring wireless network for manet node " << i);

        NodeContainer stas;
        stas.Create(mobileNodes - 1);
        // Now, create the container with all nodes on this link
        NodeContainer mobile(manet.Get(i), stas);

        WifiHelper wifimobile;
        WifiMacHelper macmobile;
        wifiPhy.SetChannel(wifiChannel.Create());
        // Create unique ssids for these networks
        std::string ssidString("wifi-mobile");
        std::stringstream ss;
        ss << i;
        ssidString += ss.str();
        Ssid ssid = Ssid(ssidString);
        // setup stas
        macmobile.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifimobile.Install(wifiPhy, macmobile, stas);
        // setup ap.
        macmobile.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifimobile.Install(wifiPhy, macmobile, manet.Get(i));
        // Collect all of these new devices
        NetDeviceContainer mobileDevices(apDevices, staDevices);

        // Add the IPv4 protocol stack to the nodes in our container
        //
        internet.Install(stas);
        //
        // Assign IPv4 addresses to the device drivers (actually to the associated
        // IPv4 interfaces) we just created.
        //
        ipAddrs.Assign(mobileDevices);
        //
        // Assign a new network prefix for each mobile network, according to
        // the network mask initialized above
        //
        ipAddrs.NewNetwork();
        //
        // The new wireless nodes need a mobility model so we aggregate one
        // to each of the nodes we just finished building.
        //
        Ptr<ListPositionAllocator> subnetAlloc = CreateObject<ListPositionAllocator>();
        for (uint32_t j = 0; j < mobile.GetN(); ++j)
        {
            subnetAlloc->Add(Vector(0.0, j, 0.0));
        }
        mobility.PushReferenceMobilityModel(manet.Get(i));
        mobility.SetPositionAllocator(subnetAlloc);
        mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                                  "Bounds",
                                  RectangleValue(Rectangle(-10, 10, -10, 10)),
                                  "Speed",
                                  StringValue("ns3::ConstantRandomVariable[Constant=3]"),
                                  "Pause",
                                  StringValue("ns3::ConstantRandomVariable[Constant=0.4]"));
        mobility.Install(stas);
    }

    ///////////////////////////////////////////////////////////////////////////
    //                                                                       //
    // Application configuration                                             //
    //                                                                       //
    ///////////////////////////////////////////////////////////////////////////

    // Create the OnOff application to send UDP datagrams of size
    // 210 bytes at a rate of 10 Kb/s, between two nodes
    // We'll send data from the first wired LAN node on the first wired LAN
    // to the last wireless STA on the last mobilestructure net, thereby
    // causing packets to traverse CSMA to adhoc to mobilestructure links

    NS_LOG_INFO("Create Applications.");
    uint16_t port = 9; // Discard port (RFC 863)

    // Let's make sure that the user does not define too few nodes
    // to make this example work.  We need lanNodes > 1  and mobileNodes > 1
    //NS_ASSERT(lanNodes > 1 && mobileNodes > 1);
    // We want the source to be the first node created outside of the manet
    // Conveniently, the variable "manetNodes" holds this node index value
    Ptr<Node> appSource = NodeList::GetNode(manetNodes);
    // We want the sink to be the last node created in the topology.
    uint32_t lastNodeIndex =
        manetNodes + manetNodes * (mobileNodes - 1) - 1;
    Ptr<Node> appSink = NodeList::GetNode(lastNodeIndex);
    // Let's fetch the IP address of the last node, which is on Ipv4Interface 1
    Ipv4Address remoteAddr = appSink->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(remoteAddr, port)));

    ApplicationContainer apps = onoff.Install(appSource);
    apps.Start(Seconds(3));
    apps.Stop(Seconds(stopTime - 1));

    // Create a packet sink to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(appSink);
    apps.Start(Seconds(3));

    ///////////////////////////////////////////////////////////////////////////
    //                                                                       //
    // Tracing configuration                                                 //
    //                                                                       //
    ///////////////////////////////////////////////////////////////////////////

    NS_LOG_INFO("Configure Tracing.");
    CsmaHelper csma;

    //
    // Let's set up some ns-2-like ascii traces, using another helper class
    //
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("hanet-compairson.tr");
    wifiPhy.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);
    internet.EnableAsciiIpv4All(stream);

    // Csma captures in non-promiscuous mode
    csma.EnablePcapAll("hanet-compairson", false);
    // pcap captures on the manet wifi devices
    wifiPhy.EnablePcap("hanet-compairson", manetDevices, false);
    // pcap trace on the application data sink
    wifiPhy.EnablePcap("hanet-compairson", appSink->GetId(), 0);

    if (useCourseChangeCallback)
    {
        Config::Connect("/NodeList/*/$ns3::MobilityModel/CourseChange",
                        MakeCallback(&CourseChangeCallback));
    }
    NS_LOG_UNCOND(lastNodeIndex);
    AnimationInterface anim("hanet-compairson.xml");

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
