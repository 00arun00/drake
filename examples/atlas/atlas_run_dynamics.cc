#include <chrono>
#include <memory>

#include <gflags/gflags.h>

#include "drake/common/find_resource.h"
#include "drake/geometry/drake_visualizer.h"
#include "drake/geometry/scene_graph.h"
#include "drake/lcm/drake_lcm.h"
#include "drake/multibody/parsing/parser.h"
#include "drake/multibody/plant/contact_results_to_lcm.h"
#include "drake/systems/analysis/simulator.h"
#include "drake/systems/analysis/simulator_gflags.h"
#include "drake/systems/framework/diagram_builder.h"
#include "drake/multibody/plant/compliant_contact_manager.h"
#include "drake/systems/analysis/simulator_print_stats.h"

#include <iostream>
#define PRINT_VAR(a) std::cout << #a ": " << a << std::endl;
#define PRINT_VARn(a) std::cout << #a ":\n" << a << std::endl;

DEFINE_double(simulation_time, 2.0, "Simulation duration in seconds");
DEFINE_double(penetration_allowance, 1.0E-3, "Allowable penetration (meters).");
DEFINE_double(stiction_tolerance, 1.0E-3,
              "Allowable drift speed during stiction (m/s).");

DEFINE_double(
    mbp_discrete_update_period, 5.0E-4,
    "The fixed-time step period (in seconds) of discrete updates for the "
    "multibody plant modeled as a discrete system. Strictly positive. "
    "Set to zero for a continuous plant model.");

namespace drake {
namespace examples {
namespace atlas {
namespace {

using drake::math::RigidTransformd;
using drake::multibody::MultibodyPlant;
using Eigen::Translation3d;
using Eigen::Vector3d;
using Eigen::VectorXd;
using drake::multibody::internal::CompliantContactManager;
using drake::multibody::internal::ManagerStats;
using clock = std::chrono::steady_clock;

int do_main() {
  if (FLAGS_mbp_discrete_update_period < 0) {
    throw std::runtime_error(
        "mbp_discrete_update_period must be a non-negative number.");
  }

  // Build a generic multibody plant.
  systems::DiagramBuilder<double> builder;
  auto [plant, scene_graph] = AddMultibodyPlantSceneGraph(
      &builder,
      std::make_unique<MultibodyPlant<double>>(
          FLAGS_mbp_discrete_update_period));

  const std::string full_name =
      FindResourceOrThrow("drake/examples/atlas/urdf/atlas_convex_hull.urdf");
  multibody::Parser(&plant).AddModelFromFile(full_name);

  // Add model of the ground.
  const double static_friction = 1.0;
  const Vector4<double> green(0.5, 1.0, 0.5, 1.0);
  plant.RegisterVisualGeometry(plant.world_body(), RigidTransformd(),
                               geometry::HalfSpace(), "GroundVisualGeometry",
                               green);
  // For a time-stepping model only static friction is used.
  const multibody::CoulombFriction<double> ground_friction(static_friction,
                                                           static_friction);
  plant.RegisterCollisionGeometry(plant.world_body(), RigidTransformd(),
                                  geometry::HalfSpace(),
                                  "GroundCollisionGeometry", ground_friction);

  plant.Finalize();
  plant.set_penetration_allowance(FLAGS_penetration_allowance);  

  // Set the speed tolerance (m/s) for the underlying Stribeck friction model
  // For two points in contact, this is the maximum allowable drift speed at the
  // edge of the friction cone, an approximation to true stiction.
  plant.set_stiction_tolerance(FLAGS_stiction_tolerance);

  auto owned_contact_manager =
      std::make_unique<CompliantContactManager<double>>();
  CompliantContactManager<double>* manager = owned_contact_manager.get();
  // contact_manager_ = owned_contact_manager.get();
  plant.SetDiscreteUpdateManager(std::move(owned_contact_manager));  
  (void)manager;

  // Sanity check model size.
  DRAKE_DEMAND(plant.num_velocities() == 36);
  DRAKE_DEMAND(plant.num_positions() == 37);

  // Verify the "pelvis" body is free and modeled with quaternions dofs before
  // moving on with that assumption.
  const drake::multibody::Body<double>& pelvis = plant.GetBodyByName("pelvis");
  DRAKE_DEMAND(pelvis.is_floating());
  DRAKE_DEMAND(pelvis.has_quaternion_dofs());
  // Since there is a single floating body, we know that the positions for it
  // lie first in the state vector.
  DRAKE_DEMAND(pelvis.floating_positions_start() == 0);
  // Similarly for velocities. The velocities for this floating pelvis are the
  // first set of velocities after all model positions, since the state vector
  // is stacked as x = [q; v].
  DRAKE_DEMAND(pelvis.floating_velocities_start() == plant.num_positions());

  // Publish contact results for visualization.
  ConnectContactResultsToDrakeVisualizer(&builder, plant, scene_graph);

  lcm::DrakeLcm lcm;
  geometry::DrakeVisualizerd::AddToBuilder(&builder, scene_graph, &lcm);
  auto diagram = builder.Build();

  // Create a context for this system:
  std::unique_ptr<systems::Context<double>> diagram_context =
      diagram->CreateDefaultContext();
  systems::Context<double>& plant_context =
      diagram->GetMutableSubsystemContext(plant, diagram_context.get());

  const VectorXd tau = VectorXd::Zero(plant.num_actuated_dofs());
  plant.get_actuation_input_port().FixValue(&plant_context, tau);

  // Set the pelvis frame P initial pose.
  const Translation3d X_WP(0.0, 0.0, 0.95);
  plant.SetFreeBodyPoseInWorldFrame(&plant_context, pelvis, X_WP);

#if 0
  const auto& hokuyo_link = plant.GetBodyByName("hokuyo_link");
  const Vector3d p_WHokuyo =
      plant.EvalBodyPoseInWorld(plant_context, hokuyo_link).translation();
  manager->AddDistanceConstraint(plant.world_body(), p_WHokuyo, hokuyo_link,
                                 Vector3d::Zero(), 0.0, 1e6, plant.time_step());

  geometry::DrakeVisualizerParams params;
  params.role = geometry::Role::kProximity;
  geometry::DrakeVisualizerd::DispatchLoadMessage(scene_graph, &lcm, params);

  diagram->Publish(*diagram_context); // I want to see the model.
#endif

  auto simulator =
      MakeSimulatorFromGflags(*diagram, std::move(diagram_context));
  clock::time_point sim_start_time = clock::now();        
  simulator->AdvanceTo(FLAGS_simulation_time);
  clock::time_point sim_end_time = clock::now();
  const double sim_time =
      std::chrono::duration<double>(sim_end_time - sim_start_time).count();
  std::cout << "AdvanceTo() time [sec]: " << sim_time << std::endl;  
  systems::PrintSimulatorStatistics(*simulator);

  const ManagerStats& stats = manager->stats();
  PRINT_VAR(stats.num_iters);
  PRINT_VAR(stats.num_ls_iters);

  return 0;
}

}  // namespace
}  // namespace atlas
}  // namespace examples
}  // namespace drake

int main(int argc, char* argv[]) {
  gflags::SetUsageMessage(
      "\nPassive simulation of the Atlas robot. With the default time step of "
      "\n1 ms this simulation typically runs slightly faster than real-time. "
      "\nThe time step has an effect on the joint limits stiffnesses, which "
      "\nconverge quadratically to the rigid limit as the time step is "
      "\ndecreased. Thus, decrease the time step for more accurately resolved "
      "\njoint limits. "
      "\nLaunch meldis before running this example.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  return drake::examples::atlas::do_main();
}
