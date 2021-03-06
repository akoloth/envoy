#include "extensions/transport_sockets/tap/config.h"

#include "envoy/config/transport_socket/tap/v2alpha/tap.pb.h"
#include "envoy/config/transport_socket/tap/v2alpha/tap.pb.validate.h"
#include "envoy/registry/registry.h"

#include "common/config/utility.h"
#include "common/protobuf/utility.h"

#include "extensions/transport_sockets/tap/tap.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tap {

Network::TransportSocketFactoryPtr UpstreamTapSocketConfigFactory::createTransportSocketFactory(
    const Protobuf::Message& message,
    Server::Configuration::TransportSocketFactoryContext& context) {
  const auto& outer_config =
      MessageUtil::downcastAndValidate<const envoy::config::transport_socket::tap::v2alpha::Tap&>(
          message);
  auto& inner_config_factory = Config::Utility::getAndCheckFactory<
      Server::Configuration::UpstreamTransportSocketConfigFactory>(
      outer_config.transport_socket().name());
  ProtobufTypes::MessagePtr inner_factory_config = Config::Utility::translateToFactoryConfig(
      outer_config.transport_socket(), inner_config_factory);
  auto inner_transport_factory =
      inner_config_factory.createTransportSocketFactory(*inner_factory_config, context);
  return std::make_unique<TapSocketFactory>(
      outer_config.file_sink().path_prefix(), outer_config.file_sink().format(),
      std::move(inner_transport_factory), context.dispatcher().timeSystem());
}

Network::TransportSocketFactoryPtr DownstreamTapSocketConfigFactory::createTransportSocketFactory(
    const Protobuf::Message& message, Server::Configuration::TransportSocketFactoryContext& context,
    const std::vector<std::string>& server_names) {
  const auto& outer_config =
      MessageUtil::downcastAndValidate<const envoy::config::transport_socket::tap::v2alpha::Tap&>(
          message);
  auto& inner_config_factory = Config::Utility::getAndCheckFactory<
      Server::Configuration::DownstreamTransportSocketConfigFactory>(
      outer_config.transport_socket().name());
  ProtobufTypes::MessagePtr inner_factory_config = Config::Utility::translateToFactoryConfig(
      outer_config.transport_socket(), inner_config_factory);
  auto inner_transport_factory = inner_config_factory.createTransportSocketFactory(
      *inner_factory_config, context, server_names);
  return std::make_unique<TapSocketFactory>(
      outer_config.file_sink().path_prefix(), outer_config.file_sink().format(),
      std::move(inner_transport_factory), context.dispatcher().timeSystem());
}

ProtobufTypes::MessagePtr TapSocketConfigFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::config::transport_socket::tap::v2alpha::Tap>();
}

REGISTER_FACTORY(UpstreamTapSocketConfigFactory,
                 Server::Configuration::UpstreamTransportSocketConfigFactory);

REGISTER_FACTORY(DownstreamTapSocketConfigFactory,
                 Server::Configuration::DownstreamTransportSocketConfigFactory);

} // namespace Tap
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
