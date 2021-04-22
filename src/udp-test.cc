#include <bits/stdint-uintn.h>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/gnuplot.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/log-macros-enabled.h"
#include "ns3/nstime.h"
#include "ns3/rng-seed-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UDP Throughput");

int
main (int argc, char *argv[])
{
  uint32_t seed = 5321;
  uint64_t run = 1;
  SeedManager::SetSeed (seed);
  SeedManager::SetRun (run);
  //
  // Enable logging for UdpClient and
  //
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  bool useV6 = false;
  Address serverAddress;
  ApplicationContainer sinkApps;
  uint16_t port = 4000;
  uint32_t PacketCount = 10000;
  uint32_t PacketSize = 1472;
  float duration = 120.0;
  bool tracing = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
  cmd.AddValue ("Seed", "Set seed", seed);
  cmd.AddValue ("run", "Number of run", run);
  cmd.AddValue ("PacketCount", "Number of Packet", PacketCount);
  cmd.AddValue ("PacketSize", "Size of Packet", PacketSize);
  cmd.AddValue ("Tracing", "Tracing with pcap", tracing);
  cmd.Parse (argc, argv);

  //
  // Topology
  //
  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
  n.Create (2);

  InternetStackHelper internet;
  internet.Install (n);

  NS_LOG_INFO ("Create channels.");
  //
  // Ethernet like channel
  //
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1000Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  //csma.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));
  NetDeviceContainer d = csma.Install (n);

  //
  // IP
  //

  NS_LOG_INFO ("Assign IP Addresses.");
  if (useV6 == false)
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer i = ipv4.Assign (d);
      serverAddress = Address (i.GetAddress (1));
      // install PacketSink on node one.
      PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (i.GetAddress (1), port));
      sinkApps = sink.Install (n.Get (1));
    }
  else
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:0000:f00d:cafe::", Ipv6Prefix (64));
      Ipv6InterfaceContainer i6 = ipv6.Assign (d);
      serverAddress = Address (i6.GetAddress (1, 1));
    }

  NS_LOG_INFO ("Create Applications.");
  //
  // Create one udpServer applications on node one.
  //
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (n.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (duration + 1.0));

  if (useV6 == false)
    {
      sinkApps.Start (Seconds (1.0));
      sinkApps.Stop (Seconds (duration + 1.0));
    }

  //
  // Create one UdpClient application to send UDP datagrams from node zero to
  // node one.
  //
  uint32_t MaxPacketSize = PacketSize;
  //Time interPacketInterval = Seconds (1e-6);
  //Time interPacketInterval = NanoSeconds(2000);
  Time interPacketInterval = MicroSeconds(2);
  uint32_t maxPacketCount = PacketCount;
  UdpClientHelper client (serverAddress, port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  apps = client.Install (n.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (duration + 2.0));

  // Pcap
  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      csma.EnablePcapAll ("udp-test", false);
    }

  // Install FlowMonitor on nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (duration));
  Simulator::Run ();
  NS_LOG_INFO ("Done.");

  // Gnuplot
  Gnuplot gnuplot = Gnuplot ("Flow vs Throughput.png");
  gnuplot.SetTerminal ("png");
  gnuplot.SetLegend ("Flow", "Throughput");
  std::ofstream outfile ("Flow vs Throughput.plt");

  Gnuplot2dDataset dataset;
  dataset.SetTitle ("Throughput");
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  // Printing flow statistics
  monitor->CheckForLostPackets ();
  if (useV6 == false)
    {
      Ptr<Ipv4FlowClassifier> classifier =
          DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
           i != stats.end (); ++i)
        {
          std::cout << "MaxPacketSize: " << PacketSize << "\n";
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow ID" << i->first << " (" << t.sourceAddress << " -> "
                    << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
		  double timeTakenClient =
              i->second.timeLastTxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
          std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / timeTakenClient / 1024 / 1024 << " Mbps\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";

          // Throughput
          double timeTaken =
              i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ();
          double Throughput = i->second.rxBytes * 8.0 / timeTaken / 1024 / 1024;

          std::cout << "  Throughput: " << Throughput << " Mbps\n";
          dataset.Add ((double) i->first, (double) Throughput);
          std::cout << "  Average Jitter:  " << i->second.jitterSum.GetSeconds() / i->second.rxPackets << " s\n";
          std::cout << "  Average Delay:  " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
          std::cout << "  Lost packets:  " << i->second.lostPackets << " \n";
          std::cout << "  Last packet sent at:  " << i->second.timeLastTxPacket.GetSeconds ()
                    << " s\n";
          std::cout << "  Last packet received at:  " << i->second.timeLastRxPacket.GetSeconds ()
                    << " s\n";
        }
    }
  else
    {

      Ptr<Ipv6FlowClassifier> classifier =
          DynamicCast<Ipv6FlowClassifier> (flowmon.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
           i != stats.end (); ++i)
        {
          std::cout << "Here"
                    << "\n";
          Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow ID" << i->first << " (" << t.sourceAddress << " -> "
                    << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1024 / 1024 << " Mbps\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";

          // Throughput
          double timeTaken =
              i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
          double Throughput = i->second.rxBytes * 8.0 / timeTaken / 1e-6;

          std::cout << "  Throughput: " << Throughput << " Mbps\n";
          dataset.Add ((double) i->first, (double) Throughput);
          std::cout << "  Average Jitter:  " << i->second.jitterSum / i->second.rxPackets << " s\n";
          std::cout << "  Average Delay:  " << i->second.delaySum / i->second.rxPackets << " s\n";
          std::cout << "  Lost packets:  " << i->second.lostPackets << " \n";
        }
    }

  gnuplot.AddDataset (dataset);
  gnuplot.GenerateOutput (outfile);
  outfile.close ();

  if (useV6 == false)
    {
      Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
      std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
    }
  Simulator::Destroy ();
}
