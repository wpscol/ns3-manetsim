// TODO:
// - throughput
// - delays
// - packet to the nearest spine node == within network/online
// - loss propagation
// - obstacles
// - terrain
// - more network configuration
// - combain scripts into all-in-one
// - calculate more values after simulation in C++

#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <vector>

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
void collectMovementData(const NodeContainer& nodes);
// Collect information about status
void collectConnectivityData(const NodeContainer& nodes);
// Selects nodes in the center to act as servers
NodeContainer selectCentralSpine(const NodeContainer& nodes, double percentage, double areaSizeX, double areaSizeY);
NodeContainer selectHorizontalSpine(const NodeContainer& nodes, double percentage, double areaSizeY);
// Check for connectivity on each node
void SniffMonitorRx(Ptr<const Packet> pkt, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu,
                    SignalNoiseDbm snr, uint16_t staId);

//
// VARIABLES
//
// consts
const uint32_t sinkPort = 8080;

// configuration
double samplingFreq = 1.0;
double simulationTime = 10.0;
double warmupTime = 1.0;
bool bPcapEnable = false;
std::string resultsPathString = "./output";

// Flow monitor
Ptr<FlowMonitor> monitor;
FlowMonitorHelper flowmon;

// Results
uint32_t movementCsvOutputIterator = 0;
std::ostringstream movementCsvOutput;

uint32_t linkStateCsvOutputIterator = 0;
std::ostringstream linkStateCsvOutput;

// States
std::vector<bool> g_isSpineNode;
std::map<uint32_t, std::set<Mac48Address>> g_neighbors;

NS_LOG_COMPONENT_DEFINE("MANETSim");

int main(int argc, char* argv[]) {
  // Components logging
  LogComponentEnable("MANETSim", LOG_LEVEL_INFO);

  // Rng configuration
  uint32_t rngSeed = 1;
  uint32_t rngRun = 1;

  // simulation region
  uint32_t nodesNum = 20;
  uint32_t spineNodesPercentage = 20;
  std::string spineVariant = "horizontal";
  double areaSizeX = 5.0;
  double areaSizeY = areaSizeX;

  // mobility configuration
  double minSpeed = 1.0;
  double maxSpeed = 3.0;

  // app configuration
  std::string wifiType = "80211ax";
  uint32_t packetsPerSecond = 10;
  uint32_t packetsSize = 512;

  // // propagation loss
  // std::string propagationLossModel = "nakagami";

  // Commandline parameters
  CommandLine cmd;
  cmd.AddValue("areaSizeX", "X axis size of the simulation area (m)", areaSizeX);
  cmd.AddValue("areaSizeY", "Y axis size of the simulation area (m)", areaSizeY);
  cmd.AddValue("maxSpeed", "Maximum speed value for random mobility (m/s)", maxSpeed);
  cmd.AddValue("minSpeed", "Minimum speed value for random mobility (m/s)", minSpeed);
  cmd.AddValue("nodesNum", "Number of nodes in the simulation", nodesNum);
  cmd.AddValue("spineNodesPercent", "Percentage of nodes working as servers (%)", spineNodesPercentage);
  cmd.AddValue("spineVariant", "Percentage of nodes working as servers: centroid | horizontal", spineVariant);
  cmd.AddValue("packetsPerSecond", "Number of packets sent every second from nodes to each spine", packetsPerSecond);
  cmd.AddValue("packetsSize", "Size of the sent packets", packetsSize);
  cmd.AddValue("wifiType", "Which Wi-Fi stack to use: 80211b | 80211g | 80211ax", wifiType);
  cmd.AddValue("resultsPath", "Path to store the simulation results", resultsPathString);
  cmd.AddValue("rngRun", "Number of the run", rngRun);
  cmd.AddValue("rngSeed", "Seed used for the simulation", rngSeed);
  cmd.AddValue("samplingFreq", "How often should measurements be taken (s)", samplingFreq);
  cmd.AddValue("simulationTime", "Duration of the simulation run (s)", simulationTime);
  cmd.AddValue("warmupTime", "Warm-up time before collecting data (s)", warmupTime);
  cmd.Parse(argc, argv);

  // Prepare results directory and path
  auto resultsPath = prepareResultsDir(resultsPathString);

  // Print configuration
  NS_LOG_INFO("MANET Simulation configuration:");
  NS_LOG_INFO("> nodesNum: " << nodesNum);
  NS_LOG_INFO("> spineNodePercent: " << spineNodesPercentage);
  NS_LOG_INFO("> spineVariant: " << spineVariant);
  NS_LOG_INFO("> packetsPerSecond: " << packetsPerSecond);
  NS_LOG_INFO("> packetsSize: " << packetsSize);
  NS_LOG_INFO("> areaSize: X=" << areaSizeX << " Y=" << areaSizeY);
  NS_LOG_INFO("> maxSpeed: " << maxSpeed);
  NS_LOG_INFO("> minSpeed: " << minSpeed);
  NS_LOG_INFO("> simulationTime: " << simulationTime);
  NS_LOG_INFO("> warmupTime: " << warmupTime);
  NS_LOG_INFO("> samplingFreq: " << samplingFreq);
  NS_LOG_INFO("> seed: " << rngSeed);
  NS_LOG_INFO("> rngRun: " << rngRun);
  NS_LOG_INFO("> resultsPath: " << resultsPath);

  // cmd.AddValue ("netanim", "Enable NetAnim", bNetAnim);
  // cmd.AddValue ("hiddenSsid", "Hide SSID in simulation", bHiddenSSID); // TODO

  // Set seed and run number
  RngSeedManager::SetSeed(rngSeed);
  RngSeedManager::SetRun(rngRun);

  // Node creation
  NodeContainer nodes;
  nodes.Create(nodesNum);

  // Setup position
  Ptr<PositionAllocator> positionAllocator = CreateObject<RandomRectanglePositionAllocator>();
  positionAllocator->SetAttribute("X", StringValue(Sprintf("ns3::UniformRandomVariable[Min=0|Max=%.2f]", areaSizeX)));
  positionAllocator->SetAttribute("Y", StringValue(Sprintf("ns3::UniformRandomVariable[Min=0|Max=%.2f]", areaSizeY)));

  // Mobility configuration
  MobilityHelper mobility;
  mobility.SetPositionAllocator(positionAllocator);

  // Configure nodes movement
  mobility.SetMobilityModel(
      "ns3::RandomWalk2dMobilityModel", "Mode", StringValue("Distance"), "Distance", DoubleValue(2.5), "Bounds",
      RectangleValue(Rectangle(0.0, areaSizeX, 0.0, areaSizeY)), "Speed",
      StringValue(Sprintf("ns3::UniformRandomVariable[Min=%.2f|Max=%.2f]", minSpeed, maxSpeed)), "Direction",
      StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.28318]"), "Time", TimeValue(Seconds(1.0)));
  mobility.Install(nodes);

  // Promote percentage of central nodes to the spine
  if (spineNodesPercentage > 100 || spineNodesPercentage < 0) {
    NS_FATAL_ERROR("Percentage value for spine nodes is incorrect: `" << spineNodesPercentage << "`");
  }

  // Promote nodes to spine
  NodeContainer spine;
  if (spineVariant == "horizontal") {
    spine = selectHorizontalSpine(nodes, spineNodesPercentage / 100.0, areaSizeY);

  } else if (spineVariant == "centroid") {
    spine = selectCentralSpine(nodes, spineNodesPercentage / 100.0, areaSizeX, areaSizeY);

  } else {
    NS_LOG_WARN("Chosen wrong spine variant: " << spineVariant << "(horizontal,centroid). Defaulting to horizontal.");
    spine = selectHorizontalSpine(nodes, spineNodesPercentage / 100.0, areaSizeY);
  }

  // Mark spine nodes with global flag
  g_isSpineNode.assign(nodesNum, false);
  for (uint32_t i = 0; i < spine.GetN(); i++) {
    uint32_t id = spine.Get(i)->GetId();
    g_isSpineNode[id] = true;
  }

  // Collect data every sammplingFreq time

  movementCsvOutput << "id,time,node,x,y,z,speed" << std::endl;
  Simulator::Schedule(Seconds(warmupTime + samplingFreq), &collectMovementData, nodes);

  linkStateCsvOutput << "id,time,node,link" << std::endl;
  Simulator::Schedule(Seconds(warmupTime + samplingFreq), &collectConnectivityData, nodes);

  // Physical layer configuration
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> channel = wifiChannel.Create();
  YansWifiPhyHelper wifiPhy;

  // TODO: To be corrected (and blessed with buildings)
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel");
  wifiPhy.SetChannel(channel);

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  WifiHelper wifi;
  if (wifiType == "80211b") {
    wifi.SetStandard(WIFI_STANDARD_80211b);

  } else if (wifiType == "80211g") {
    wifi.SetStandard(WIFI_STANDARD_80211g);

  } else if (wifiType == "80211ax") {
    wifi.SetStandard(WIFI_STANDARD_80211ax);

  } else {
    NS_FATAL_ERROR("Unknown wifiType \"" << wifiType << "\"");
  }

  // TODO: Configure network parameters

  // adhoc mac configuration
  // wifiPhy.Set("TxPowerStart", DoubleValue(8.0));
  // wifiPhy.Set("TxPowerEnd", DoubleValue(8.0));
  // wifiPhy.Set("TxGain", DoubleValue(0));
  // wifiPhy.Set("RxGain", DoubleValue(0));
  // wifiPhy.Set("RxNoiseFigure", DoubleValue(7));
  wifiPhy.Set("ChannelSettings", StringValue("{0, 80, BAND_5GHZ, 0}"));
  // wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

  // wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");

  // wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
  //                             "DataMode", StringValue("DsssRate11Mbps"),
  //                             "ControlMode", StringValue("DsssRate1Mbps"));

  // RTS/CTS configuration

  // configure hidden/shown ssid

  // configure network devices
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Configure sniffer
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
                                MakeCallback(&SniffMonitorRx));

  // install network protocols stack
  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  // ip configuration
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Install packet sink server on the spine nodes
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = sinkHelper.Install(spine);

  // Start server after warmup period
  sinkApps.Start(Seconds(warmupTime));
  sinkApps.Stop(Seconds(warmupTime + simulationTime));

  // client data rate
  std::ostringstream dataRateStr;
  dataRateStr << (packetsPerSecond * packetsSize * spine.GetN()) * 8 << "bps";

  // Configure clients sending packets
  OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
  clientHelper.SetAttribute("PacketSize", UintegerValue(packetsSize));
  clientHelper.SetAttribute("DataRate", StringValue(dataRateStr.str()));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(warmupTime + 0.2)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(warmupTime + simulationTime)));

  // Install client apps
  for (uint32_t i = 0; i < nodes.GetN(); i++) {
    // If spine, continue
    Ptr<Node> n = nodes.Get(i);
    if (g_isSpineNode[n->GetId()]) {
      continue;
    }

    // Install sender application to each spine node
    for (uint32_t i = 0; i < spine.GetN(); i++) {
      Ipv4Address spineAddr = interfaces.GetAddress(spine.Get(i)->GetId());
      AddressValue remoteAddr(InetSocketAddress(spineAddr, sinkPort));
      clientHelper.SetAttribute("Remote", remoteAddr);
      clientHelper.Install(n);
    }
  }

  // // PCAP capture
  // if (bPcapEnable) {
  //   wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  //   wifiPhy.EnablePcap("adhoc-simulation", devices);
  //   // TODO: Add per devices enable
  // }

  // // NetAnim
  // if (bNetAnim) {
  //     AnimationInterface anim("adhoc-simulation.xml");
  //     // TODO: Add per node configuration
  //     // anim.SetConstantPosition(nodes.Get(0), 10, 10);
  //     // anim.SetConstantPosition(nodes.Get(1), 20, 20);
  // }

  // Declare stopping time
  Simulator::Stop(Seconds(warmupTime + simulationTime));

  // Configure flow monitor
  monitor = flowmon.InstallAll();

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
  std::filesystem::path movementTargetPath = resultsPath / std::filesystem::path("movement.csv");
  std::ofstream movementOutputFile(movementTargetPath);
  movementOutputFile << movementCsvOutput.str();
  NS_LOG_INFO("Movement results saved to: " << movementTargetPath);

  std::filesystem::path conntargetPath = resultsPath / std::filesystem::path("connectivity.csv");
  std::ofstream connOutputFile(conntargetPath);
  connOutputFile << linkStateCsvOutput.str();
  NS_LOG_INFO("Connectivity results saved to: " << conntargetPath);

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

// Get data of the nodes in specified point in time
void collectMovementData(const NodeContainer& nodes) {
  for (uint32_t i = 0; i < nodes.GetN(); i++) {
    Ptr<Node> n = nodes.Get(i);
    Ptr<MobilityModel> mob = nodes.Get(i)->GetObject<MobilityModel>();

    // Spacial data collection
    Vector pos = mob->GetPosition();
    Vector vel = mob->GetVelocity();
    double speed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);

    Time simNowTime = Simulator::Now();

    // Mark as spine if it is
    std::string nodeName = std::to_string(i) + (g_isSpineNode[n->GetId()] ? "S" : "");

    movementCsvOutput << movementCsvOutputIterator++ << ',' << simNowTime.GetSeconds() << ',' << nodeName << ','
                      << pos.x << ',' << pos.y << ',' << pos.z << ',' << speed << std::endl;
  }

  Simulator::Schedule(Seconds(samplingFreq), &collectMovementData, nodes);
}

// Conectivity data
void collectConnectivityData(const NodeContainer& nodes) {
  Time simNowTime = Simulator::Now();

  for (uint32_t i = 0; i < nodes.GetN(); i++) {
    bool linkUp = !g_neighbors[nodes.Get(i)->GetId()].empty();
    linkStateCsvOutput << linkStateCsvOutputIterator++ << ',' << simNowTime.GetSeconds() << ',' << nodes.Get(i)->GetId()
                       << "," << linkUp << std::endl;
    // clear for next interval
    g_neighbors[nodes.Get(i)->GetId()].clear();
  }

  Simulator::Schedule(Seconds(samplingFreq), &collectConnectivityData, nodes);
}

// Centroid variant
NodeContainer selectCentralSpine(const NodeContainer& nodes, double percentage, double areaSizeX, double areaSizeY) {
  const uint32_t N = nodes.GetN();
  const uint32_t spineCount = std::max<uint32_t>(1, static_cast<uint32_t>(std::round(percentage * N)));

  // compute centroid
  double cx = areaSizeX * 0.5;
  double cy = areaSizeY * 0.5;

  // distance of each node to centroid
  std::vector<std::pair<double, uint32_t>> dists;
  dists.reserve(N);
  for (uint32_t i = 0; i < N; ++i) {
    Vector pos = nodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
    double dx = pos.x - cx;
    double dy = pos.y - cy;
    dists.emplace_back(dx * dx + dy * dy, i);
  }

  // sort ascending, take first spineCount
  std::sort(dists.begin(), dists.end(), [](auto& a, auto& b) { return a.first < b.first; });

  NodeContainer spine;
  for (uint32_t i = 0; i < spineCount; ++i) {
    spine.Add(nodes.Get(dists[i].second));
  }
  return spine;
}

// Y axis center variant
NodeContainer selectHorizontalSpine(const NodeContainer& nodes, double percentage, double areaSizeY) {
  const uint32_t N = nodes.GetN();
  const uint32_t spineCount = std::max<uint32_t>(1u, static_cast<uint32_t>(std::round(percentage * N)));

  // Compute the target y‑line
  const double centerY = areaSizeY * 0.5;

  // Build a vector of (verticalDistance, nodeIndex)
  std::vector<std::pair<double, uint32_t>> dists;
  dists.reserve(N);
  for (uint32_t i = 0; i < N; ++i) {
    Vector pos = nodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
    double dy = (pos.y >= centerY) ? (pos.y - centerY) : (centerY - pos.y);
    dists.emplace_back(dy, i);
  }

  // Sort by ascending vertical distance
  std::sort(dists.begin(), dists.end(), [](auto const& a, auto const& b) { return a.first < b.first; });

  // Pick the first spineCount nodes
  NodeContainer spine;
  for (uint32_t i = 0; i < spineCount; ++i) {
    spine.Add(nodes.Get(dists[i].second));
  }
  return spine;
}

// Check for connectivity traces
void SniffMonitorRx(Ptr<const Packet> pkt, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu,
                    SignalNoiseDbm snr, uint16_t staId) {
  uint32_t thisNode = Simulator::GetContext();

  // extract sender MAC from the 80211 header
  WifiMacHeader hdr;
  pkt->PeekHeader(hdr);
  Mac48Address sender = hdr.GetAddr2();
  g_neighbors[thisNode].insert(sender);
}