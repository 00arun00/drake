#include "iiwa_simulation.h"

#include "drake/systems/LCMSystem.h"
#include "drake/Path.h"

namespace drake {
namespace examples {
namespace kuka_iiwa_arm {

using Drake::RigidBodySystem;

std::shared_ptr<RigidBodySystem> CreateIIWAArmSystem(void) {
  // Initializes LCM.
  std::shared_ptr<lcm::LCM> lcm = std::make_shared<lcm::LCM>();

  // Instantiates a rigid body system and adds the robot arm to it.
  auto rigid_body_sys = std::allocate_shared<RigidBodySystem>(
      Eigen::aligned_allocator<RigidBodySystem>());

  rigid_body_sys->addRobotFromFile(
      Drake::getDrakePath() + "/examples/kuka_iiwa_arm/urdf/iiwa14.urdf",
      DrakeJoint::FIXED);

  rigid_body_sys->penetration_stiffness = 3000.0;
  rigid_body_sys->penetration_damping = 0;

  return rigid_body_sys;
}

void SetupWorld(const std::shared_ptr<RigidBodyTree>& tree, double box_width,
                double box_depth) {
  // Adds the ground.
  DrakeShapes::Box geom(Eigen::Vector3d(box_width, box_width, box_depth));
  Eigen::Isometry3d T_element_to_link = Eigen::Isometry3d::Identity();
  T_element_to_link.translation() << 0, 0,
      -box_depth / 2.0;  // top of the box is at z = 0
  RigidBody& world = tree->world();
  Eigen::Vector4d color;
  color << 0.9297, 0.7930, 0.6758,
      1;  // was hex2dec({'ee','cb','ad'})'/256 in matlab
  world.addVisualElement(
      DrakeShapes::VisualElement(geom, T_element_to_link, color));
  tree->addCollisionElement(
      RigidBody::CollisionElement(geom, T_element_to_link, &world), world,
      "terrain");
  tree->updateStaticCollisionElements();
}

Drake::SimulationOptions SetupSimulation(void) {
  // Specifies the simulation options.
  Drake::SimulationOptions options;
  options.realtime_factor = 0;  // As fast as possible.
  options.initial_step_size = 0.002;

  // Prevents exception from being thrown when simulation runs slower than real
  // time, which it most likely will given the small step size.
  options.warn_real_time_violation = true;
  return (options);
}

}  // namespace kuka_iiwa_arm
}  // namespace examples
}  // namespace drake
