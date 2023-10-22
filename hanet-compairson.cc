
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
NS_LOG_COMPONENT_DEFINE("MixedWireless");

int
main(int argc, char* argv[])
{
    int nodeSpeed = 20; // in m/s
    int nodePause = 0; 
    int movilNodes = 3;
    uint32_t stopTime = 100;
    //declaring routing protocols
    Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("1472"));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("100kb/s"));

    AodvHelper aodv;
    OlsrHelper olsr;
    DsdvHelper dsdv;
    DsrHelper dsr;
    DsrMainHelper dsrMain;
    Ipv4ListRoutingHelper list;
    InternetStackHelper internet;

    //
    // First, we declare and initialize a few local variables that control some
    // simulation parameters.
    //

    uint32_t manetNodes = 10;
    //DECLARE m_protocolName
    std::string m_protocolName = "DSR";
    //uint32_t routingProtocol;
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

    //setting mobility model
        MobilityHelper mobilityAdhoc;
    int64_t streamIndex = 0; // used to get consistent mobility across scenarios

    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
    pos.Set("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
    pos.Set("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"));

    Ptr<PositionAllocator> taPositionAlloc = pos.Create()->GetObject<PositionAllocator>();
    streamIndex += taPositionAlloc->AssignStreams(streamIndex);

    std::stringstream ssSpeed;
    ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
    std::stringstream ssPause;
    ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
    mobilityAdhoc.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                   "Speed",
                                   StringValue(ssSpeed.str()),
                                   "Pause",
                                   StringValue(ssPause.str()),
                                   "PositionAllocator",
                                   PointerValue(taPositionAlloc));
    mobilityAdhoc.SetPositionAllocator(taPositionAlloc);
    mobilityAdhoc.Install(adhocContainer);
    streamIndex += mobilityAdhoc.AssignStreams(adhocContainer, streamIndex);
//routing protocols for networks, it depends on the distance between nodes.
    if (m_protocolName == "OLSR")
    {
        list.Add(olsr, 100);
        internet.SetRoutingHelper(list);
        internet.Install(adhocContainer);
    }
    else if (m_protocolName == "AODV")
    {
        list.Add(aodv, 100);
        internet.SetRoutingHelper(list);
        internet.Install(adhocContainer);
    }
    else if (m_protocolName == "DSDV")
    {
        list.Add(dsdv, 100);
        internet.SetRoutingHelper(list);
        internet.Install(adhocContainer);
    }
    else if (m_protocolName == "DSR")
    {
        internet.Install(adhocContainer);
        dsrMain.Install(dsr, adhocContainer);
    }
        else
    {
        NS_FATAL_ERROR("No such protocol:" << m_protocolName);
    }
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("192.168.0.0", "255.255.255.0");
    ipAddrs.Assign(manetDevices);
    ipAddrs.SetBase("172.16.0.0", "255.255.255.0");
    for (uint32_t i = 0; i < manetNodes; ++i)
    {
        NS_LOG_INFO("Configuring wireless network for manet node " << i);
        //
        // Create a container to manage the nodes of the LAN.  We need
        // two containers here; one with all of the new nodes, and one
        // with all of the nodes including new and existing nodes
        //
        NodeContainer stas;
        stas.Create(movilNodes - 1);
        // Now, create the container with all nodes on this link
        NodeContainer movil(adhocContainer.Get(i), stas);
        //
        // Create an infrastructure network
        //
        WifiHelper wifimovil;
        WifiMacHelper macmovil;
        wifiPhy.SetChannel(wifiChannel.Create());
        // Create unique ssids for these networks
        std::string ssidString("wifi-movil");
        std::stringstream ss;
        ss << i;
        ssidString += ss.str();
        Ssid ssid = Ssid(ssidString);
        // setup stas
        macmovil.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifimovil.Install(wifiPhy, macmovil, stas);
        // setup ap.
        macmovil.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifimovil.Install(wifiPhy, macmovil, adhocContainer.Get(i));
        // Collect all of these new devices
        NetDeviceContainer movilDevices(apDevices, staDevices);

        // Add the IPv4 protocol stack to the nodes in our container
        //
        internet.Install(stas);
        //
        // Assign IPv4 addresses to the device drivers (actually to the associated
        // IPv4 interfaces) we just created.
        //
        ipAddrs.Assign(movilDevices);
        //
        // Assign a new network prefix for each movil network, according to
        // the network mask initialized above
        //
        ipAddrs.NewNetwork();
        //
        // The new wireless nodes need a mobility model so we aggregate one
        // to each of the nodes we just finished building.
        //
        Ptr<ListPositionAllocator> subnetAlloc = CreateObject<ListPositionAllocator>();
        for (uint32_t j = 0; j < movil.GetN(); ++j)
        {
            subnetAlloc->Add(Vector(0.0, j, 0.0));
        }
        
        mobilityAdhoc.PushReferenceMobilityModel(adhocContainer.Get(i));
        mobilityAdhoc.SetPositionAllocator(subnetAlloc);
        mobilityAdhoc.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                                  "Bounds",
                                  RectangleValue(Rectangle(-10, 10, -10, 10)),
                                  "Speed",
                                  StringValue("ns3::ConstantRandomVariable[Constant=3]"),
                                  "Pause",
                                  StringValue("ns3::ConstantRandomVariable[Constant=0.4]"));
        mobilityAdhoc.Install(stas);
    }
    NS_LOG_INFO("Create Applications.");
    
    //create application to send data from the first movilNOde to the last movilNode
    //create source and sink
    
    // we want the first node created in the topology to be the source.
    
    /*
    Ptr<Node> appSource = NodeList::GetNode(10);
    // We want the sink to be the last node created in the topology.
    uint32_t lastNodeIndex = manetNodes + manetNodes * (movilNodes - 1) - 1;
    NS_LOG_UNCOND(lastNodeIndex);
    Ptr<Node> appSink = NodeList::GetNode(11);
    uint16_t port = 9;
    Ipv4Address remoteAddr = appSink->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(remoteAddr, port)));

    ApplicationContainer apps = onoff.Install(appSource);
    apps.Start(Seconds(3));
    apps.Stop(Seconds(stopTime - 1));

   // Create a packet sink to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(appSink);
    apps.Start(Seconds(3));
    */

   NS_LOG_INFO("Configure Tracing.");
    CsmaHelper csma;

    //
    // Let's set up some ns-2-like ascii traces, using another helper class
    //
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("hanet-tracing.tr");
    wifiPhy.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);
    internet.EnableAsciiIpv4All(stream);

    // Csma captures in non-promiscuous mode
    csma.EnablePcapAll("hanet", false);
    // pcap captures on the backbone wifi devices
    wifiPhy.EnablePcap("hanet", manetDevices, false);
    // pcap trace on the application data sink
    wifiPhy.EnablePcap("hanet", appSink->GetId(), 0);

    AnimationInterface anim("hanet.xml");
    NS_LOG_INFO("Run Simulation.");

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;


}