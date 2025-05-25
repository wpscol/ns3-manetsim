// TODO:
// - eavesdropping scenario (control device power)
// - jamming scenaio (high intensity signal/jamming model)
// - OFDMA
// - Power Control
// - removing nodes

#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
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
// Collect sent and received packets
void TxLogger(Ptr<const Packet> pkt);
void RxLogger(Ptr<const Packet> pkt, const Address& from);

// Control node status
void BringNodeDown(Ptr<Node> node);
void BringNodeUp(Ptr<Node> node);

// Wipe simulation step
void wipeStep(const NodeContainer& nodes);

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
uint32_t movementCsvOutputIterator, linkStateCsvOutputIterator = 0;
std::ostringstream movementCsvOutput, linkStateCsvOutput;

uint32_t packetsCsvIterator = 0;
std::ostringstream packetsCsv;

// States
std::vector<bool> g_isSpineNode;
std::map<uint32_t, std::set<Mac48Address>> g_neighbors;
std::vector<bool> g_isUp;

std::string wipeDirection = "E";
double wipePosX = 0.0;
double wipePosY = 0.0;
double wipeInit = false;
double wipeSpeed = '1';
double simAreaX = 0.0;
double simAreaY = 0.0;

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

  std::string environment = "none";
  std::string scenario = "none";

  // forest
  uint32_t treeCount = 20;
  double treeHeight = 5;
  double treeSize = 0.5;

  // // urban
  // uint32_t buildingGridWidth = 3;
  // double buildingSize = 7.0;
  // double buildingSpacing = 6.0;

  // mobility configuration
  double minSpeed = 1.0;
  double maxSpeed = 3.0;

  // app configuration
  uint32_t packetsPerSecond = 10;
  uint32_t packetsSize = 512;
  uint32_t wifiChannelWidth = 80;

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
  cmd.AddValue("wifiChannelWidth", "Size of the WiFi channel: 20 | 40 | 80 | 160 (MHz)", wifiChannelWidth);
  cmd.AddValue("resultsPath", "Path to store the simulation results", resultsPathString);
  cmd.AddValue("rngRun", "Number of the run", rngRun);
  cmd.AddValue("rngSeed", "Seed used for the simulation", rngSeed);
  cmd.AddValue("samplingFreq", "How often should measurements be taken (every X s)", samplingFreq);
  cmd.AddValue("simulationTime", "Duration of the simulation run (s)", simulationTime);
  cmd.AddValue("warmupTime", "Warm-up time before collecting data (s)", warmupTime);
  cmd.AddValue("environment", "Choose target environment for testing: none | forest", environment);
  cmd.AddValue("treeCount", "Number of trees in simulation [forest environment only]", treeCount);
  cmd.AddValue("treeSize", "Size of the single tree (m) [forest environment only]", treeSize);
  cmd.AddValue("treeHeight", "Height of the single tree (m) [forest environment only]", treeHeight);
  cmd.AddValue("scenario", "Specify target simulation scenario: none | wipe", scenario);
  cmd.AddValue("wipeDirection",
               "Specify the direction from which to slowly stop nodes: (N)orth | (E)ast | (S)outh | (W)est | (R)andom",
               wipeDirection);
  cmd.AddValue("wipeSpeed", "Declare how fast should the wipe line move (m/s)", wipeSpeed);

  // // cmd.AddValue("buildingGridWidth", "Number of buildings per row [urban environment only]", buildingGridWidth);
  // // cmd.AddValue("buildingSize", "Building side length (m) [urban environment only]", buildingSize);
  // // cmd.AddValue("buildingSpacing", "buildingSpacing between buildings (m) [urban environment only]",
  // buildingSpacing);
  cmd.Parse(argc, argv);

  // Prepare results directory and path
  auto resultsPath = prepareResultsDir(resultsPathString);

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
  positionAllocator->SetAttribute("Z", StringValue("1.5"));

  // Mobility configuration
  MobilityHelper mobility;
  mobility.SetPositionAllocator(positionAllocator);

  // Configure nodes movement
  // without walls
  mobility.SetMobilityModel(
      "ns3::RandomWalk2dMobilityModel", "Mode", StringValue("Distance"), "Distance", DoubleValue(2.5), "Bounds",
      RectangleValue(Rectangle(0.0, areaSizeX, 0.0, areaSizeY)), "Speed",
      StringValue(Sprintf("ns3::UniformRandomVariable[Min=%.2f|Max=%.2f]", minSpeed, maxSpeed)), "Direction",
      StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.28318]"), "Time", TimeValue(Seconds(1.0)));

  // aware of walls
  // mobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel", "Mode", StringValue("Distance"), "Distance",
  //                           DoubleValue(2.5), "Bounds", RectangleValue(Rectangle(0, areaSizeX, 0, areaSizeY)),
  //                           "Speed", StringValue(Sprintf("ns3::UniformRandomVariable[Min=%.2f|Max=%.2f]", minSpeed,
  //                           maxSpeed)), "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.28318]"),
  //                           "Time", TimeValue(Seconds(1.0)));

  // Install mobility
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

  // Mark all nodes online (online by default)
  g_isUp.assign(nodesNum, true);

  for (uint32_t i = 0; i < spine.GetN(); i++) {
    uint32_t id = spine.Get(i)->GetId();
    g_isUp[id] = true;
    g_isSpineNode[id] = true;
  }

  // List spine nodes
  std::ostringstream nodesList;
  for (uint32_t i = 0; i < spine.GetN(); i++) {
    nodesList << spine.Get(i)->GetId() << " ";
  }

  // Print configuration
  NS_LOG_INFO("MANET Simulation configuration:");
  NS_LOG_INFO("> nodesNum: " << nodesNum);
  NS_LOG_INFO("> spineNodePercent: " << spineNodesPercentage);
  NS_LOG_INFO("> spineNodeCount: " << spine.GetN());
  NS_LOG_INFO("> spineNodeNumbers: " << nodesList.str());
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

  NS_LOG_INFO("> environment" << environment);
  if (environment == "forest") {
    NS_LOG_INFO("> treeCount: " << treeCount);
    NS_LOG_INFO("> treeSize: " << treeSize);
    NS_LOG_INFO("> treeHeight" << treeHeight);
  }

  NS_LOG_INFO("> scenario" << scenario);
  if (scenario == "wipe") {
    NS_LOG_INFO("> wipeDirection: " << wipeDirection);
    NS_LOG_INFO("> wipeSpeed: " << wipeSpeed);
  }

  // if (environment == "urban") {
  //   NS_LOG_INFO("> buildingGridWidth: " << buildingGridWidth);
  //   NS_LOG_INFO("> buildingSize: " << buildingSize);
  //   NS_LOG_INFO("> buildingSpacing" << buildingSpacing);
  // }

  // Configure wipe simulation
  if (scenario == "wipe") {
    if (wipeDirection != "N" && wipeDirection != "E" && wipeDirection != "S" && wipeDirection != "W" &&
        wipeDirection != "R") {
      NS_FATAL_ERROR("Incorrect wipe direction, expeced value N,E,S,W,R, but provided: `" << wipeDirection << "`");
    }

    simAreaX = areaSizeX;
    simAreaY = areaSizeY;

    Simulator::Schedule(Seconds(warmupTime), &wipeStep, nodes);
  }

  // Collect data every sammplingFreq time
  movementCsvOutput << "id,time,node,x,y,z,speed" << std::endl;
  Simulator::Schedule(Seconds(warmupTime + samplingFreq), &collectMovementData, nodes);

  linkStateCsvOutput << "id,time,node,l2_link,online" << std::endl;
  Simulator::Schedule(Seconds(warmupTime + samplingFreq), &collectConnectivityData, nodes);

  packetsCsv << "id,time,node,uid,size,received" << std::endl;

  // Physical layer configuration
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> channel = wifiChannel.Create();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(channel);
  wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

  if (environment == "forest") {
    // Configure propagation
    Ptr<LogDistancePropagationLossModel> logLoss = CreateObject<LogDistancePropagationLossModel>();
    logLoss->SetPathLossExponent(4.5);
    Ptr<NakagamiPropagationLossModel> nakagami = CreateObject<NakagamiPropagationLossModel>();
    nakagami->SetNext(logLoss);
    channel->SetPropagationLossModel(nakagami);

    // Randomly place trees on in the area
    Ptr<UniformRandomVariable> uvX = CreateObject<UniformRandomVariable>();
    uvX->SetAttribute("Min", DoubleValue(0.0));
    uvX->SetAttribute("Max", DoubleValue(areaSizeX));

    Ptr<UniformRandomVariable> uvY = CreateObject<UniformRandomVariable>();
    uvY->SetAttribute("Min", DoubleValue(0.0));
    uvY->SetAttribute("Max", DoubleValue(areaSizeY));

    for (uint32_t i = 0; i < treeCount; ++i) {
      Ptr<Building> tree = CreateObject<Building>();
      double x = uvX->GetValue();
      double y = uvY->GetValue();
      tree->SetBoundaries(Box(x, x + treeSize, y, y + treeSize, 0.0, treeHeight));
    }

  } else {
    NS_LOG_INFO("Unspecified environment “" << environment << "”, using defaults");
  }

  // if (environment == "urban") {
  //   // Configure propagation
  //   Ptr<LogDistancePropagationLossModel> logLoss = CreateObject<LogDistancePropagationLossModel>();
  //   logLoss->SetPathLossExponent(3.0);
  //   Ptr<HybridBuildingsPropagationLossModel> buildingLoss = CreateObject<HybridBuildingsPropagationLossModel>();
  //   buildingLoss->SetNext(logLoss);

  //   channel->SetPropagationLossModel(buildingLoss);

  //   // Configure buildings grid
  //   Ptr<GridBuildingAllocator> gridAlloc = CreateObject<GridBuildingAllocator>();
  //   gridAlloc->SetAttribute("GridWidth", UintegerValue(buildingGridWidth));
  //   gridAlloc->SetAttribute("LengthX", DoubleValue(buildingSize));
  //   gridAlloc->SetAttribute("LengthY", DoubleValue(buildingSize));
  //   gridAlloc->SetAttribute("DeltaX", DoubleValue(buildingSpacing + buildingSize));
  //   gridAlloc->SetAttribute("DeltaY", DoubleValue(buildingSpacing + buildingSize));
  //   gridAlloc->SetAttribute("Height", DoubleValue(8.0));

  //   gridAlloc->SetBuildingAttribute("NFloors", UintegerValue(1));
  //   gridAlloc->SetBuildingAttribute("NRoomsX", UintegerValue(3));
  //   gridAlloc->SetBuildingAttribute("NRoomsY", UintegerValue(3));
  //   gridAlloc->SetBuildingAttribute("Type", StringValue("Residential"));
  //   gridAlloc->SetBuildingAttribute("ExternalWallsType", StringValue("ConcreteWithWindows"));

  //   gridAlloc->SetAttribute("MinX", DoubleValue(0.0));
  //   gridAlloc->SetAttribute("MinY", DoubleValue(0.0));

  //   gridAlloc->Create(buildingGridWidth * buildingGridWidth);

  // Install objects for all nodes
  BuildingsHelper::Install(nodes);

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax);
  if (wifiChannelWidth != 20 && wifiChannelWidth != 40 && wifiChannelWidth != 80 && wifiChannelWidth != 160) {
    NS_FATAL_ERROR("Incorrect WiFi channel width: " << wifiChannelWidth);
  }
  wifiPhy.Set("ChannelSettings", StringValue("{0, " + std::to_string(wifiChannelWidth) + ", BAND_5GHZ, 0}"));

  // TODO: Configure network parameters

  // adhoc mac configuration
  // wifiPhy.Set("TxPowerStart", DoubleValue(8.0));
  // wifiPhy.Set("TxPowerEnd", DoubleValue(8.0));
  // wifiPhy.Set("TxGain", DoubleValue(0));
  // wifiPhy.Set("RxGain", DoubleValue(0));
  // wifiPhy.Set("RxNoiseFigure", DoubleValue(7));
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
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = sinkHelper.Install(spine);

  // Start server after warmup period
  sinkApps.Start(Seconds(warmupTime));
  sinkApps.Stop(Seconds(warmupTime + simulationTime));

  // client data rate
  std::ostringstream dataRateStr;
  dataRateStr << (packetsPerSecond * packetsSize * spine.GetN()) * 8 << "bps";

  // Configure clients sending packets
  OnOffHelper clientHelper("ns3::UdpSocketFactory", Address());
  clientHelper.SetAttribute("PacketSize", UintegerValue(packetsSize));
  clientHelper.SetAttribute("DataRate", StringValue(dataRateStr.str()));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(warmupTime + 0.2)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(warmupTime + simulationTime)));

  // Send packets from each node to the spine (except to itself if spine)
  for (uint32_t i = 0; i < nodes.GetN(); i++) {
    Ptr<Node> n = nodes.Get(i);
    uint32_t srcId = n->GetId();

    // Iterate *all* spine nodes:
    for (uint32_t j = 0; j < spine.GetN(); j++) {
      Ptr<Node> s = spine.Get(j);
      uint32_t dstId = s->GetId();

      // Skip if this is the same node (i.e. spine sending to itself)
      if (srcId == dstId) {
        continue;
      }

      // Otherwise install one OnOff client from n → this spine
      Ipv4Address spineAddr = interfaces.GetAddress(dstId);
      AddressValue remoteAddr(InetSocketAddress(spineAddr, sinkPort));
      clientHelper.SetAttribute("Remote", remoteAddr);
      clientHelper.Install(n);
    }
  }

  // Trace every transmit from *any* OnOffApplication
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/Tx", MakeCallback(&TxLogger));

  // Trace every receive at *any* PacketSink
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&RxLogger));

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

  //
  // Save results to the files
  //
  std::filesystem::path movementTargetPath = resultsPath / std::filesystem::path("movement.csv");
  std::ofstream movementOutputFile(movementTargetPath);
  movementOutputFile << movementCsvOutput.str();
  NS_LOG_INFO("Movement results saved to: " << movementTargetPath);

  std::filesystem::path conntargetPath = resultsPath / std::filesystem::path("connectivity.csv");
  std::ofstream connOutputFile(conntargetPath);
  connOutputFile << linkStateCsvOutput.str();
  NS_LOG_INFO("Connectivity results saved to: " << conntargetPath);

  std::filesystem::path packetsTargetPath = resultsPath / std::filesystem::path("packets.csv");
  std::ofstream packetsOutputFile(packetsTargetPath);
  packetsOutputFile << packetsCsv.str();
  NS_LOG_INFO("Packets catched saved to: " << packetsTargetPath);

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
    bool linkUp = !g_neighbors[nodes.Get(i)->GetId()].empty() && g_isUp[nodes.Get(i)->GetId()];
    bool isUp = g_isUp[nodes.Get(i)->GetId()];
    linkStateCsvOutput << linkStateCsvOutputIterator++ << ',' << simNowTime.GetSeconds() << ',' << nodes.Get(i)->GetId()
                       << "," << linkUp << "," << isUp << std::endl;
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
  std::sort(dists.begin(), dists.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

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

// sent
void TxLogger(Ptr<const Packet> pkt) {
  double t = Simulator::Now().GetSeconds();
  uint32_t nodeId = Simulator::GetContext();
  std::string nodeName = std::to_string(nodeId) + (g_isSpineNode[nodeId] ? "S" : "");

  // time,node,uid,size
  packetsCsv << packetsCsvIterator++ << "," << t << "," << nodeName << "," << pkt->GetUid() << ',' << pkt->GetSize()
             << ',' << 0 << std::endl;
}

// received
void RxLogger(Ptr<const Packet> pkt, const Address& from) {
  double t = Simulator::Now().GetSeconds();
  uint32_t nodeId = Simulator::GetContext();
  std::string nodeName = std::to_string(nodeId) + (g_isSpineNode[nodeId] ? "S" : "");

  // time,node,uid,size
  packetsCsv << packetsCsvIterator++ << "," << t << "," << nodeName << "," << pkt->GetUid() << "," << pkt->GetSize()
             << ',' << 1 << std::endl;
}

// Stop node
void BringNodeDown(Ptr<Node> node) {
  uint32_t id = node->GetId();
  g_isUp[id] = false;

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  ipv4->SetDown(1);
  NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s: Node " << id << " interface DOWN");
}

// Start node
void BringNodeUp(Ptr<Node> node) {
  uint32_t id = node->GetId();
  g_isUp[id] = true;

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  ipv4->SetUp(1);
  NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s: Node " << id << " interface UP");
}

// Advance wipe line and bring nodes down when crossed
void wipeStep(const NodeContainer& nodes) {
  double t = Simulator::Now().GetSeconds();
  // initialize wipePos on first call
  if (!wipeInit) {
    if (wipeDirection == "N") {
      wipePosY = 0.0;
    } else if (wipeDirection == "S") {
      wipePosY = simAreaY;
    } else if (wipeDirection == "E") {
      wipePosX = 0.0;
    } else if (wipeDirection == "W") {
      wipePosX = simAreaX;
    } else /* R */ {
      // random cardinal
      std::vector<std::string> dirs = {"N", "E", "S", "W"};
      wipeDirection = dirs[std::rand() % 4];
      wipeInit = false; // re-init next tick
    }
    wipeInit = true;
  }

  // move the wipe line
  if (wipeDirection == "N") {
    wipePosY += wipeSpeed * samplingFreq;
  } else if (wipeDirection == "S") {
    wipePosY -= wipeSpeed * samplingFreq;
  } else if (wipeDirection == "E") {
    wipePosX += wipeSpeed * samplingFreq;
  } else if (wipeDirection == "W") {
    wipePosX -= wipeSpeed * samplingFreq;
  }

  // check each node
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> n = nodes.Get(i);
    if (!g_isUp[n->GetId()])
      continue; // already down
    Ptr<MobilityModel> mob = n->GetObject<MobilityModel>();
    Vector pos = mob->GetPosition();

    bool crossed = false;
    if (wipeDirection == "N" && pos.y <= wipePosY)
      crossed = true;
    if (wipeDirection == "S" && pos.y >= wipePosY)
      crossed = true;
    if (wipeDirection == "E" && pos.x <= wipePosX)
      crossed = true;
    if (wipeDirection == "W" && pos.x >= wipePosX)
      crossed = true;

    if (crossed) {
      BringNodeDown(n);
    }
  }

  // schedule next step until end of simulation
  if (t < warmupTime + simulationTime) {
    Simulator::Schedule(Seconds(samplingFreq), &wipeStep, nodes);
  }
}