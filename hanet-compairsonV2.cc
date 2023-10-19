
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
    int movilNodes = 2;
    //declaring routing protocols
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
    std::string m_protocolName = "OLSR";
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
    for (uint32_t i = 0; i < manetNodes; ++i) {
        for (uint32_t j = i + 1; j < manetNodes; ++j) {
            double distance = CalculateDistance(adhocContainer.Get(i), adhocContainer.Get(j));

            if (distance < 100) {
                m_protocolName = "OLSR";
            } else if (distance < 200) {
                m_protocolName = "AODV";
            } else if (distance < 300) {
                m_protocolName = "DSDV";
            } else {
                m_protocolName = "DSR";
            }

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
        }
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

}


double CalculateDistance(Ptr<Node> node1, Ptr<Node> node2) {
    Ptr<MobilityModel> mobility1 = node1->GetObject<MobilityModel>();
    Ptr<MobilityModel> mobility2 = node2->GetObject<MobilityModel>();

    Vector pos1 = mobility1->GetPosition();
    Vector pos2 = mobility2->GetPosition();

    return pos1.DistanceFrom(pos2);
}