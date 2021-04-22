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

NS_LOG_COMPONENT_DEFINE ("TCP Throughput");

int
main (int argc, char *argv[])
{
  uint32_t seed = 5321;
  uint64_t run = 1;
  SeedManager::SetSeed (seed);
  SeedManager::SetRun (run);
  uint32_t packetSize = 1472;
  uint32_t packetCount = 100;
  uint16_t port = 9;
  bool useV6 = false;
  float duration = 120.0;
  uint32_t sendSize = 512;
  bool tracing = true;

  Address serverAddress;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
  cmd.AddValue ("Seed", "Set seed", seed);
  cmd.AddValue ("run", "Number of run", run);
  cmd.AddValue ("packetCount", "Total Number of bytes", packetCount);
  cmd.AddValue ("Duration", "Duration", duration);
  cmd.AddValue ("SendSize", "The amount of data to send each time", sendSize);
  cmd.AddValue ("Tracing", "Tracing with pcap", tracing);
  cmd.Parse (argc, argv);

  uint32_t maxBytes = packetSize * packetCount;
  //
  // Network Topology
  //
  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
  n.Create (2);

  InternetStackHelper internet;
  internet.Install (n);

  NS_LOG_INFO ("Create channels.");
  //
  // Create Channel similar to Ethernet
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
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (d);
  serverAddress = Address (i.GetAddress (1));

  NS_LOG_INFO ("Create Applications.");
  //
  // Use BulkSendHelper to creat app and install on node0

  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (i.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  source.SetAttribute ("SendSize", UintegerValue (sendSize));
  ApplicationContainer sourceApps = source.Install (n.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (duration));

  //
  // Create a PacketSinkApplication and install it on node 1
  //
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (n.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (duration));

  // Pcap
  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      csma.EnablePcapAll ("tcp-test", false);
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
          std::cout << "  First Rx:   " << i->second.timeFirstRxPacket.GetSeconds() << "\n";
          std::cout << "  Last Rx:   " << i->second.timeLastRxPacket.GetSeconds() << "\n";

          // Throughput
          double timeTaken =
              i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ();
          double Throughput = i->second.rxBytes * 8.0 / timeTaken / 1024 / 1024;

          std::cout << "  Throughput: " << Throughput << " Mbps\n";
          dataset.Add ((double) i->first, (double) Throughput);
          std::cout << "  Average Jitter:  " << i->second.jitterSum.GetSeconds()/ i->second.rxPackets << " s\n";
          std::cout << "  Average Delay:  " << i->second.delaySum.GetSeconds()/ i->second.rxPackets << " s\n";
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
              i->second.timeLastTxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
          double Throughput = i->second.rxBytes * 8.0 / timeTaken / 1024 / 1024;

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

  // PacketSink
  if (useV6 == false)
    {
      Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
      std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
    }

  Simulator::Destroy ();
}
