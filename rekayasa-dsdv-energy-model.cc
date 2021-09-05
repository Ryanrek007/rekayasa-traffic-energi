#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/energy-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/onoff-application.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RekayasaTraffic");
NodeContainer networkNodes;
NetDeviceContainer devices;
Ipv4InterfaceContainer interfaces;

std::map<Ptr<Socket>, Ipv4InterfaceAddress> m_socketAddresses;

int port = 9;
int bytesTotal; ///< total bytes yang diterima setiap node
int packetsReceived; ///< total packets yang diterima setiap node
std::string CSVfileName = "rekayasa-dsdv-energy-model.csv";

void
ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet ;
  Address senderaddress;

  while ((packet = socket->RecvFrom (senderaddress))) 
    {
      bytesTotal += packet->GetSize ();
      packetsReceived += 1;
      if (packet->GetSize () > 0)
        {
          InetSocketAddress addr = InetSocketAddress::ConvertFrom(senderaddress);
          NS_LOG_UNCOND ( Simulator::Now ().GetSeconds () 
                          << "S " << socket->GetNode()->GetId()
                          << " Received one packet from " << addr.GetIpv4()
                          << " Packet Size " << packet->GetSize()
                          << " packet uid " << packet ->GetUid()
                          << " port: " << addr.GetPort()
                         );    
        } 
    }
}

void
CheckThroughput ()
{
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;

  std::ofstream out (CSVfileName.c_str (), std::ios::app);

  out << (Simulator::Now ()).GetSeconds () << "," 
                                    << kbs << "," 
                                    << packetsReceived << "," 
                                    << std::endl;

  out.close ();
  packetsReceived = 0;
  Simulator::Schedule (Seconds (1.0), CheckThroughput);
}

Ptr <Socket>
SetupPacketReceive (Ipv4Address addr, Ptr <Node> node)
{
  Ptr <Socket> sink = Socket::CreateSocket (node,ns3::UdpSocketFactory::GetTypeId ());  
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local); // berfungsi untuk mengikat antara ip address dengan port
  sink->SetRecvCallback (MakeCallback (&ReceivePacket));
  return sink;
}

/// Trace function for remaining energy at node.
void
RemainingEnergy (double oldValue, double remainingEnergy)
{
  if (remainingEnergy > 500) /// energy > 500
  {
  NS_LOG_UNCOND (
                  Simulator::Now ().GetSeconds () << "s " <<
                  remainingEnergy << " ==> lebih dari 500 "
                  );
  }
  else if (remainingEnergy < 500){ //energy < 500
  NS_LOG_UNCOND (
                  Simulator::Now ().GetSeconds () << "s " <<
                  remainingEnergy << " ==> kurang dari 500"
                  );
  }
}

/// Trace function for total energy consumption at node.
void
TotalEnergy (double oldValue, double totalEnergy)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
                 << "s Total energy consumed by radio = " << totalEnergy << "J");
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RekayasaTraffic", LogLevel (LOG_PREFIX_TIME | LOG_PREFIX_NODE | LOG_LEVEL_INFO));
  LogComponentEnable("DsdvRoutingProtocol", LogLevel (LOG_PREFIX_TIME | LOG_LOGIC | LOG_FUNCTION | LOG_ERROR));
  
  double startTime = 0.0;       // seconds
  double totalTime = 1000.0;       // seconds
  int n_node = 20;
  int n_dest = 2;
  std::string rate ("40kbps"); //40,000 bit per second = 5000Bps =5KBps
  std::string phyMode ("DsssRate5_5Mbps"); /// DIRECT SEQUENCE SPREAD SPECTRUM, 22 MHZ
  int nodeSpeed = 10; // in m/s
  double energyinitial = 1000.0; //Joule
  uint32_t periodicUpdateInterval = 15; /// second
  uint32_t settlingTime = 6; ///second
  bool verbose = false;

  int count = 0;
  int counter= 0;
  double energyremaining;
  double energyConsumed;
  double sumenergyremaining;
  double sumenergyconsumsed;

  CommandLine cmd;
  cmd.AddValue ("nWifis", "Number of wifi nodes[Default:20]", n_node);
  cmd.AddValue ("nDest", "Number of wifi sink nodes[Default:2]", n_dest);
  cmd.AddValue ("startTime", "Simulation start time", startTime);
  cmd.AddValue ("totalTime", "Total Simulation time[Default:1000]", totalTime);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);  
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name[Default:DsdvManetExample.csv]", CSVfileName);
  cmd.AddValue ("verbose", "Turn on all device log components", verbose);
  cmd.Parse (argc, argv);

  std::ofstream out (CSVfileName.c_str ());
  out << "SimulationSecond," <<
  "ReceiveRate," <<
  "PacketsReceived," <<
  std::endl;
  out.close (); 

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",
                      StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                      StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (rate));  ///<DEFINISI CBR

  std::stringstream ss;
  ss << n_node;
  std::string t_nodes = ss.str ();

  std::stringstream ss3;
  ss3 << totalTime;
  std::string t_totalTime = ss3.str ();
  std::string tr_name = "rekayasa-dsdv-energy-model_ " + t_nodes+ "_Nodes_" + t_totalTime + "_s";
  
  // NodeContainer networkNodes;
  networkNodes.Create (n_node);     // create 2 nodes
  NS_ASSERT_MSG (n_node > n_dest, "node tujuan harus > n_node");

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();
    }
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  /** Wifi PHY **/
  /***************************************************************************/
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();

  /** wifi channel **/
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  
  // create wifi channel
  Ptr<YansWifiChannel> wifiChannelPtr = wifiChannel.Create ();
  wifiPhy.SetChannel (wifiChannelPtr);

  /** MAC layer **/
  // Add a MAC and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",
                                StringValue (phyMode), "ControlMode",
                                StringValue (phyMode));
  // Set it to ad-hoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");

  /** install PHY + MAC **/
  // NetDeviceContainer devices;
  devices = wifi.Install (wifiPhy, wifiMac, networkNodes);

  /** mobility **/
  MobilityHelper mobility;
  ObjectFactory coordinate;
  coordinate.SetTypeId ("ns3::RandomRectanglePositionAllocator"); //topologi
  coordinate.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));
  coordinate.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));

  std::ostringstream speedConstantRandomVariableStream;
  speedConstantRandomVariableStream << "ns3::ConstantRandomVariable[Constant="
                                    << nodeSpeed
                                    << "]";

  Ptr <PositionAllocator> taPositionAlloc = coordinate.Create ()->GetObject <PositionAllocator> ();
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel", 
                                "Speed", StringValue (speedConstantRandomVariableStream.str ()), //kecepatan
                                "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"), //pause time 2 detik
                                "PositionAllocator", PointerValue (taPositionAlloc)); 
  mobility.SetPositionAllocator (taPositionAlloc);
  mobility.Install (networkNodes);

  /** Energy Model **/
  /***************************************************************************/
  /* energy source */
  BasicEnergySourceHelper basicSourceHelper;
  // configure energy source
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (energyinitial));
  // install source
  EnergySourceContainer sources = basicSourceHelper.Install (networkNodes);
  /* device energy model */
  WifiRadioEnergyModelHelper radioEnergyHelper;
  // configure radio energy model
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.380));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.313));
  radioEnergyHelper.Set ("CcaBusyCurrentA", DoubleValue (0.273));
  radioEnergyHelper.Set ("SwitchingCurrentA", DoubleValue (0.273));
  radioEnergyHelper.Set ("IdleCurrentA", DoubleValue (0.273));
  radioEnergyHelper.Set ("SleepCurrentA", DoubleValue (0.033));
  // install device model
  DeviceEnergyModelContainer deviceModels;
  deviceModels = radioEnergyHelper.Install (devices, sources);
  /***************************************************************************/

  /** Internet stack **/
  DsdvHelper dsdv;
  dsdv.Set ("PeriodicUpdateInterval", TimeValue (Seconds (periodicUpdateInterval)));
  dsdv.Set ("SettlingTime", TimeValue (Seconds (settlingTime)));

  InternetStackHelper stack;
  stack.SetRoutingHelper (dsdv);
  stack.Install (networkNodes);

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");

  interfaces = ipv4.Assign (devices);

  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ((tr_name + ".routes"), std::ios::out);
  dsdv.PrintRoutingTableAllEvery (Seconds (periodicUpdateInterval), routingStream);    

  for (int j = 0; j <= n_dest - 1; j++ ) //2
    {
      Ptr<Socket> sink = SetupPacketReceive (interfaces.GetAddress(j), networkNodes.Get(j));
      std::cout << sink->GetNode()->GetId()  <<" socket id\n";
    }   

  OnOffHelper onoff1 ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (0), port));
  onoff1.SetAttribute ("PacketSize", StringValue("512")); 
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  OnOffHelper onoff2 ("ns3::UdpSocketFactory",InetSocketAddress (interfaces.GetAddress (1), port));
  onoff2.SetAttribute ("PacketSize", StringValue("1024")); 
  onoff2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  ApplicationContainer apps1 = onoff1.Install (networkNodes.Get (n_dest + 0));
  Ptr<UniformRandomVariable> var1 = CreateObject<UniformRandomVariable> ();
  apps1.Start(Seconds (var1->GetValue (startTime , startTime+1)));
  apps1.Stop (Seconds (totalTime));

  ApplicationContainer apps2 = onoff2.Install (networkNodes.Get (n_dest + 1));
  Ptr<UniformRandomVariable> var2 = CreateObject<UniformRandomVariable> ();
  apps2.Start (Seconds (var2->GetValue (startTime , startTime+1)));
  apps2.Stop (Seconds (totalTime));  

  /** simulation setup **/
  // start traffic
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll (); 

  CheckThroughput();

  Simulator::Stop (Seconds(totalTime));
  Simulator::Run ();

  NS_LOG_UNCOND("\n");

 /*BERGUNA UNTUK MENGHITUNG ENERGI SISA SETELAH SIMULASI SELESAI PADA SETIAP NODE */
    for (EnergySourceContainer::Iterator iter= sources.Begin (); iter != sources.End (); ++ iter )
    {
      count++;
      energyremaining = (*iter)->GetRemainingEnergy ();      
      NS_LOG_UNCOND ("End of simulation (" << Simulator::Now ().GetSeconds ()
                      << "s) Node:"  << (*iter)->GetNode()->GetId()
                      << " current Remaining energy = " << energyremaining 
                      << "J"
                      );
      NS_ASSERT_MSG (energyremaining <= energyinitial, "sisa energi harus kurang dari energi awal");
      sumenergyremaining += energyremaining;
    }

 /*BERGUNA UNTUK MENGHITUNG TOTAL ENERGI YANG DIPAKAI SELAMA SIMULASI PADA SETIPAP NODE */
  for (DeviceEnergyModelContainer::Iterator iter = deviceModels.Begin (); iter != deviceModels.End (); ++ iter )
      {
        energyConsumed = (*iter)->GetTotalEnergyConsumption ();
        NS_LOG_UNCOND ("End of simulation (" << Simulator::Now ().GetSeconds ()
                        << "s) Node: " << counter++
                        << " Total energy consumed by radio = " << energyConsumed 
                        << "J"
                         );
        NS_ASSERT (energyConsumed <= energyinitial);
        sumenergyconsumsed += energyConsumed;
      }

  //nama output flowmon, histogram, probe detail.
  flowmon->SerializeToXmlFile (tr_name + ".flowmon", false, false);

  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();
  double TotPacketSent = 0;
  double TotPacketsRcvd = 0;
  double TotBytesRcvd = 0;
  double TotDelaySum = 0;
  double AveThroughput = 0;
  double Totpacketloss= 0;
  int counts = 0;

  for  (std::map<FlowId, FlowMonitor::FlowStats>::iterator iter = stats.begin (); iter != stats.end (); ++ iter)
  {
    NS_LOG_UNCOND("\nFlow ID: " << iter->first
                  <<"\nTx Packets = " << iter->second.txPackets
                  <<"\nRx Packets = " << iter->second.rxPackets
                  <<"\nAverage Delay = " << (iter->second.delaySum.GetSeconds()/iter->second.rxPackets) * 1000 << " ms"
                  <<"\nThroughput = " << ((iter->second.txBytes * 8) / (iter->second.timeLastTxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000) << " kbps"
                  );
    
    counts++;
    TotPacketsRcvd += iter->second.rxPackets;
    TotPacketSent += iter->second.txPackets;
    TotBytesRcvd += iter->second.rxBytes;
    Totpacketloss += iter->second.txPackets - iter->second.rxPackets;
    TotDelaySum += iter->second.delaySum.GetSeconds();
    AveThroughput += (iter->second.txBytes * 8) / (iter->second.timeLastTxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds());
  
  }
  NS_LOG_UNCOND ("\n====  SIMULATION RESULT  ====" << 
                 "\nPackets sent = " << TotPacketSent << 
                 "\nPackets Received = " << TotPacketsRcvd << 
                 "\nPackets lost = " << Totpacketloss << 
                 "\n\nPacket Delivery Ratio (PDR) = " << (TotPacketsRcvd/TotPacketSent) * 100 << "%" << 
                 "\nAverage Delay = " << (TotDelaySum / TotPacketsRcvd) * 1000 << " ms" <<
                 "\nThroughput = " << (AveThroughput /counts) /1000 << " kbps" 
  );

  NS_LOG_UNCOND ("\nSum of remaining energy: "<< sumenergyremaining << 
               "\naverage of remaining energy: " << sumenergyremaining / count << 
               "\n\nSum of consumsed energy: "<< sumenergyconsumsed << 
               "\naverage of consumsed energy: " << sumenergyconsumsed / counter <<
               "\n======================================\n"
  );
  Simulator::Destroy ();
  return 0;
}
