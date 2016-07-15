#include "drake/systems/lcm/lcm_subscriber_system.h"

#include <iostream>

#include "drake/systems/framework/basic_state_vector.h"

namespace drake {
namespace systems {
namespace lcm {

using std::make_unique;

LcmSubscriberSystem::LcmSubscriberSystem(
    const std::string& channel, const LcmBasicVectorTranslator& translator,
    LcmReceiveThread* lcm_receive_thread)
    : channel_(channel),
      translator_(translator),
      lcm_receive_thread_(lcm_receive_thread),
      basic_vector_(translator.get_basic_vector_size()) {
  // Initializes the communication layer.
  ::lcm::Subscription* sub = lcm_receive_thread_->get_lcm()->subscribe(
      channel_, &LcmSubscriberSystem::HandleMessage, this);
  sub->setQueueCapacity(1);
}

LcmSubscriberSystem::~LcmSubscriberSystem() {}

std::string LcmSubscriberSystem::get_name() const {
  return "LcmSubscriberSystem::" + channel_;
}

std::unique_ptr<Context<double>> LcmSubscriberSystem::CreateDefaultContext()
    const {
  // Creates a new context for this system and sets the number of input ports
  // to be zero. It leaves the context's state uninitialized since this system
  // does not use it.
  std::unique_ptr<Context<double>> context(new Context<double>());
  context->SetNumInputPorts(0);

  // Returns this system's context.
  return context;
}

std::unique_ptr<SystemOutput<double>> LcmSubscriberSystem::AllocateOutput()
    const {
  // Instantiates a BasicVector object and stores it in a managed pointer.
  std::unique_ptr<BasicVector<double>> data =
      make_unique<BasicVector<double>>(translator_.get_basic_vector_size());

  // Instantiates an OutputPort with the above BasicVector as the data type.
  std::unique_ptr<OutputPort<double>> port =
      make_unique<OutputPort<double>>(std::move(data));

  // Stores the above-defined OutputPort in this system output.
  std::unique_ptr<SystemOutput<double>> output(new SystemOutput<double>);
  output->ports.push_back(std::move(port));

  // Returns this system's output.
  return output;
}

void LcmSubscriberSystem::EvalOutput(const Context<double>& context,
                                     SystemOutput<double>* output) const {
  BasicVector<double>& output_vector = dynamic_cast<BasicVector<double>&>(
      *output->ports[0]->GetMutableVectorData());

  data_mutex_.lock();
  output_vector.set_value(basic_vector_.get_value());
  data_mutex_.unlock();
}

void LcmSubscriberSystem::HandleMessage(const ::lcm::ReceiveBuffer* rbuf,
                                        const std::string& channel) {
  if (channel == channel_) {
    data_mutex_.lock();
    translator_.TranslateLcmToBasicVector(rbuf, &basic_vector_);
    data_mutex_.unlock();
  } else {
    std::cerr << "LcmSubscriberSystem: HandleMessage: WARNING: Received a "
              << "message for channel \"" << channel
              << "\" instead of channel \"" << channel_ << "\". Ignoring it."
              << std::endl;
  }
}

}  // namespace lcm
}  // namespace systems
}  // namespace drake
