/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 SUE-Sim Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef APPLICATION_DEPLOYER_H
#define APPLICATION_DEPLOYER_H

#include "parameter-config.h"
#include "topology-builder.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/sue-client.h"
#include "ns3/sue-server.h"
#include "ns3/traffic-generator.h"
#include "ns3/traffic-generator-trace.h"
#include "ns3/traffic-generator-config.h"
#include "ns3/load-balancer.h"
#include "ns3/performance-logger.h"
#include "ns3/soft-ue-observer-helper.h"
#include "ns3/soft-ue-module.h"
#include <memory>
#include <vector>

namespace ns3 {

// Forward declaration
class TopologyBuilder;

/**
 * \brief Application deployer for SUE simulation
 *
 * This class is responsible for deploying and configuring all applications
 * in the SUE simulation including servers, clients, traffic generators,
 * and load balancers.
 */
class ApplicationDeployer
{
public:
    struct TruthPlannedOperation
    {
        uint64_t jobId{0};
        uint16_t msgId{0};
        uint32_t srcXpuId{0};
        uint32_t srcPort{0};
        uint32_t dstXpuId{0};
        uint32_t dstPort{0};
        OpType opType{OpType::SEND};
        uint32_t payloadBytes{0};
        double startOffsetSeconds{0.0};
        bool delayedReceive{false};
        uint32_t receiveDelayMs{0};
        bool expectValidationFailure{false};
        bool forceMultiChunk{false};
        std::string plannerMode;
        std::string pressureProfile;
    };

    /**
     * \brief Constructor
     */
    ApplicationDeployer ();

    /**
     * \brief Destructor
     */
    ~ApplicationDeployer ();

    /**
     * \brief Deploy all applications on the topology
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     */
    void DeployApplications (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);
    bool HasSoftUeObserver () const;
    SoftUeObserverHelper* GetSoftUeObserver ();
    const SoftUeObserverHelper* GetSoftUeObserver () const;
    const std::vector<TruthPlannedOperation>& GetTruthPlannedOperations () const;
    std::string GetTruthPlannerMode () const;

private:
    /**
     * \brief Install server applications on all XPU ports
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     */
    void InstallServers (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);

    /**
     * \brief Install client applications and traffic generators
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     */
    void InstallClientsAndTrafficGenerators (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);
    void InstallSoftUeTruthScenario (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);

    /**
     * \brief Create and configure a SUE client
     * \param xpuIdx XPU index (0-based)
     * \param sueIdx SUE index within XPU (0-based)
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     * \return Pointer to created SUE client
     */
    Ptr<SueClient> CreateSueClient (uint32_t xpuIdx, uint32_t sueIdx,
                                   const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);

    /**
     * \brief Create and configure a load balancer for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param sueClientsForXpu Vector of SUE clients for this XPU
     * \param config Simulation configuration parameters
     * \return Pointer to created load balancer
     */
    Ptr<LoadBalancer> CreateLoadBalancer (uint32_t xpuIdx,
                                         const std::vector<Ptr<SueClient>>& sueClientsForXpu,
                                         const SueSimulationConfig& config);

    /**
     * \brief Create and configure a traffic generator for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param loadBalancer Pointer to load balancer
     * \param config Simulation configuration parameters
     * \return Pointer to created traffic generator (Application*)
     */
    Ptr<TrafficGenerator> CreateTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                           const SueSimulationConfig& config);

    /**
     * \brief Create and configure a trace-based traffic generator for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param loadBalancer Pointer to load balancer
     * \param config Simulation configuration parameters
     * \return Pointer to created trace traffic generator (as Application*)
     */
    Ptr<TraceTrafficGenerator> CreateTraceTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                const SueSimulationConfig& config);

    /**
     * \brief Create and configure a configurable traffic generator for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param loadBalancer Pointer to load balancer
     * \param config Simulation configuration parameters
     * \return Pointer to created configurable traffic generator (as Application*)
     */
    Ptr<ConfigurableTrafficGenerator> CreateConfigurableTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                               const SueSimulationConfig& config);

    /**
     * \brief Parse fine-grained traffic configuration file
     * \param config Simulation configuration parameters
     * \return Vector of parsed traffic flows
     */
    std::vector<FineGrainedTrafficFlow> ParseFineGrainedTrafficConfig (const SueSimulationConfig& config);
    std::vector<TruthPlannedOperation> BuildTruthPlan (const SueSimulationConfig& config,
                                                       TopologyBuilder& topologyBuilder);
    std::vector<TruthPlannedOperation> BuildSemanticDemoTruthPlan (const SueSimulationConfig& config,
                                                                   TopologyBuilder& topologyBuilder);
    std::vector<TruthPlannedOperation> BuildUniformTruthPlan (const SueSimulationConfig& config,
                                                              TopologyBuilder& topologyBuilder);
    std::vector<TruthPlannedOperation> BuildStripedTruthPlan (const SueSimulationConfig& config,
                                                              TopologyBuilder& topologyBuilder);
    std::vector<TruthPlannedOperation> BuildFabricTruthPlan (const SueSimulationConfig& config,
                                                             TopologyBuilder& topologyBuilder);
    std::vector<TruthPlannedOperation> BuildFineGrainedTruthPlan (const SueSimulationConfig& config,
                                                                  TopologyBuilder& topologyBuilder);
    std::vector<TruthPlannedOperation> BuildTraceTruthPlan (const SueSimulationConfig& config,
                                                            TopologyBuilder& topologyBuilder);
    OpType SelectTruthOpType (const SueSimulationConfig& config, uint32_t ordinal) const;
    uint32_t GetTruthPayloadBytes (const SueSimulationConfig& config) const;
    bool ShouldDelayTruthReceive (const SueSimulationConfig& config, OpType opType, uint32_t ordinal) const;
    uint32_t GetTruthReceiveDelayMs (const SueSimulationConfig& config, uint32_t ordinal) const;
    double GetTruthOpSpacingSeconds (const SueSimulationConfig& config) const;
    std::unique_ptr<SoftUeObserverHelper> m_softUeObserver;
    std::vector<TruthPlannedOperation> m_truthPlannedOperations;
    std::string m_truthPlannerMode;
};

} // namespace ns3

#endif /* APPLICATION_DEPLOYER_H */
