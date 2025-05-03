#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include <filesystem>
// #include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

// Utils
// https://stackoverflow.com/a/63121929
template <typename... Args> std::string Sprintf(const char* fmt, Args... args) {
  const size_t n = snprintf(nullptr, 0, fmt, args...);
  std::vector<char> buf(n + 1);
  snprintf(buf.data(), n + 1, fmt, args...);
  return std::string(buf.data());
}

//
// HELPER FUNCTIONS
//
// Prepare fs path for the logs
std::filesystem::path prepareResultsDir(const std::string& path);
// Collect each node position to the log
void collectNodesPositions(const NodeContainer& nodes);

//
// VARIABLES
//
// configuration
float samplingFreq = 1.0;
float simulationTime = 10.0;
float warmupTime = 3.0;
bool bPcapEnable = false;
std::string resultsPathString = "./results";

// Flow monitor
Ptr<FlowMonitor> monitor;
FlowMonitorHelper flowmon;

// Results
uint32_t csvIterator = 0;
std::ostringstream csvOutput;

NS_LOG_COMPONENT_DEFINE("MANETSim");

int main(int argc, char* argv[]) {
  // Components logging
  LogComponentEnable("MANETSim", LOG_LEVEL_INFO);

  // simulation region
  uint32_t nodesNum = 5;
  float areaSizeX = 5.0;
  float areaSizeY = areaSizeX;

  // mobility configuration
  float minSpeed = 1.0;
  float maxSpeed = 5.0;

  // Commandline parameters
  CommandLine cmd;
  cmd.AddValue("nodesNum", "Number of nodes in the simulation", nodesNum);
  cmd.AddValue("areaSizeX", "X axis of the simulation area (m)", areaSizeX);
  cmd.AddValue("areaSizeY", "Y axis of the simulation area (m)", areaSizeY);
  cmd.AddValue("minSpeed", "Minimum speed value for random mobility (m/s)", minSpeed);
  cmd.AddValue("maxSpeed", "Maximum speed value for random mobility (m/s)", maxSpeed);
  cmd.AddValue("samplingFreq", "How often should messurements be taken (s)", samplingFreq);
  cmd.AddValue("simulationTime", "How long should simulation run (s)", simulationTime);
  cmd.AddValue("warmupTime", "How long should simulation wait before collecting data (s)", warmupTime);
  cmd.AddValue("resultsPath", "Path with results", resultsPathString);
  cmd.Parse(argc, argv);

  // Prepare results directory and path
  auto resultsPath = prepareResultsDir(resultsPathString);

  // Print configuration
  NS_LOG_INFO("MANET Simulation configuration:");
  NS_LOG_INFO("> nodesNum: " << nodesNum);
  NS_LOG_INFO("> areaSize: X=" << areaSizeX << " Y=" << areaSizeY);
  NS_LOG_INFO("> mobilitySpeed: " << minSpeed << "-" << maxSpeed);
  NS_LOG_INFO("> simulationTime: " << simulationTime);
  NS_LOG_INFO("> warmupTime: " << warmupTime);
  NS_LOG_INFO("> resultsPath: " << resultsPath);

  // cmd.AddValue ("netanim", "Enable NetAnim", bNetAnim);
  // cmd.AddValue ("hiddenSsid", "Hide SSID in simulation", bHiddenSSID); // TODO

  // Node creation
  NodeContainer nodes;
  nodes.Create(nodesNum);

  // Setup position
  Ptr<PositionAllocator> positionAllocator = CreateObject<RandomRectanglePositionAllocator>();
  positionAllocator->SetAttribute("X", StringValue(Sprintf("ns3::UniformRandomVariable[Min=0|Max=%.2f]", areaSizeX)));
  positionAllocator->SetAttribute("Y", StringValue(Sprintf("ns3::UniformRandomVariable[Min=0|Max=%.2f]", areaSizeY)));

  // Mobility configuration
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel", "Speed",
                            StringValue(Sprintf("ns3::UniformRandomVariable[Min=%.2f|Max=%.2f]", minSpeed, maxSpeed)),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.75]"), "PositionAllocator",
                            PointerValue(positionAllocator));
  mobility.SetPositionAllocator(positionAllocator);
  mobility.Install(nodes);

  // Collect position every sammplingFreq time
  csvOutput << "id,time,node,x,y,z" << std::endl;
  Simulator::Schedule(Seconds(warmupTime + samplingFreq), &collectNodesPositions, nodes);

  // Physical layer configuration
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> channel = wifiChannel.Create();
  YansWifiPhyHelper wifiPhy;

  // Loss configuation // TODO: Check here
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  // wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel");

  wifiPhy.SetChannel(channel);

  // WiFi channel configuration
  // Signal manipulation
  wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
  wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));
  wifiPhy.Set("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set("TxGain", DoubleValue(0));
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("RxNoiseFigure", DoubleValue(7));
  wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

  // adhoc mac configuration
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  // wifi 802.11b
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager"); // Linux one

  // wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
  //                             "DataMode", StringValue("DsssRate11Mbps"),
  //                             "ControlMode", StringValue("DsssRate1Mbps"));

  // // RTS/CTS configuration
  // Config::SetDefault(
  //     "ns3::MinstrelWifiManager::RtsCtsThreshold",
  //     bRtsCts ? UintegerValue(100) : UintegerValue(4'692'480)
  // );

  // configure hidden/shown ssid

  // configure network devices
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // install network protocols stack
  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  // ip configuration
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // flow monitor
  monitor = flowmon.InstallAll();

  // // Install server on all nodes
  // for (uint32_t i = 0; i < nodesNum; ++i) {
  //   UdpEchoServerHelper echoServer(9); // same port for all
  //   ApplicationContainer serverApps = echoServer.Install(nodes.Get(i));
  //   serverApps.Start(Seconds(1.0));
  //   serverApps.Stop(Seconds(10.0));
  // }

  // // Install clients on all nodes (each node sends to the next one)
  // for (uint32_t i = 0; i < nodesNum; ++i) {
  //   uint32_t dest = (i + 1) % nodesNum; // circular
  //   UdpEchoClientHelper echoClient(interfaces.GetAddress(dest), 9);
  //   echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  //   echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  //   echoClient.SetAttribute("PacketSize", UintegerValue(512));

  //   ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
  //   clientApps.Start(Seconds(2.0));
  //   clientApps.Stop(Seconds(10.0));
  // }

  // PCAP capture
  if (bPcapEnable) {
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("adhoc-simulation", devices);
    // TODO: Add per devices enable
  }

  // // NetAnim
  // if (bNetAnim) {
  //     AnimationInterface anim("adhoc-simulation.xml");
  //     // TODO: Add per node configuration
  //     // anim.SetConstantPosition(nodes.Get(0), 10, 10);
  //     // anim.SetConstantPosition(nodes.Get(1), 20, 20);
  // }

  // Configure flow monitor
  flowmon.InstallAll();

  // Start simulation
  Simulator::Stop(Seconds(warmupTime + simulationTime));

  // Collect time
  auto start = std::chrono::high_resolution_clock::now();

  // Run simulation
  NS_LOG_INFO("Starting simulation...");
  Simulator::Run();

  // Record time
  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;

  // Clean-up
  Simulator::Destroy();

  // Print final info
  NS_LOG_INFO("Finished in " << elapsed.count() << "!");

  // save to the file
  std::filesystem::path targetPath = resultsPath / std::filesystem::path("movement.csv");
  std::ofstream outputFile(targetPath);
  outputFile << csvOutput.str();
  NS_LOG_INFO("Results saved to: " << targetPath);

  return 0;
}

//
// Prepare path for the results
//
std::filesystem::path prepareResultsDir(const std::string& path) {
  std::filesystem::path base(path);
  std::filesystem::create_directories(base);

  return base;
}

// Get position of the nodes in specified time
void collectNodesPositions(const NodeContainer& nodes) {
  for (uint32_t i = 0; i < nodes.GetN(); i++) {
    Ptr<MobilityModel> mob = nodes.Get(i)->GetObject<MobilityModel>();
    Vector pos = mob->GetPosition();
    Time simNowTime = Simulator::Now();
    csvOutput << csvIterator++ << ',' << simNowTime.GetSeconds() << ',' << i << ',' << pos.x << ',' << pos.y << ','
              << pos.z << std::endl;
  }

  Simulator::Schedule(Seconds(samplingFreq), &collectNodesPositions, nodes);
}