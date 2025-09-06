#include "metrics_exporter.hpp"
#include <stdexcept>

using namespace prometheus;

const MetricsExporter::Labels MetricsExporter::DEFAULT_LABELS = {
    {"service", "heartbeat_server"},
    {"version", "1.0.0"}
};

MetricsExporter::MetricsExporter(const std::string& bind_address) 
    : registry_(std::make_shared<Registry>()),
      exposer_(std::make_unique<Exposer>(bind_address)),
      // Initialize gauges
      active_connections_(BuildGauge()
          .Name("heartbeat_active_connections")
          .Help("Number of active connections")
          .Register(*registry_).Add({})),
      // Initialize counters
      messages_received_(BuildCounter()
          .Name("heartbeat_messages_received_total")
          .Help("Total number of messages received")
          .Register(*registry_).Add({})),
      bytes_received_(BuildCounter()
          .Name("heartbeat_bytes_received_total")
          .Help("Total bytes received")
          .Register(*registry_).Add({})),
      errors_total_(BuildCounter()
          .Name("heartbeat_errors_total")
          .Help("Total number of errors")
          .Register(*registry_).Add({{ "type", "unknown" }})),
      // Initialize histogram
      processing_time_histogram_(BuildHistogram()
          .Name("heartbeat_processing_time_seconds")
          .Help("Message processing time in seconds")
          .Register(*registry_)
          .Add({}, Histogram::BucketBoundaries{
              0.00001, 0.00005, 0.0001, 0.0005, 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0})) {
    
    // Register metrics with the exposer
    exposer_->RegisterCollectable(registry_);
}

void MetricsExporter::update_connections(int count) {
    active_connections_.Set(count);
}

void MetricsExporter::record_message(size_t bytes, double processing_time_ns) {
    messages_received_.Increment();
    bytes_received_.Increment(bytes);
    processing_time_histogram_.Observe(processing_time_ns / 1e9); // Convert ns to seconds
}

void MetricsExporter::record_error(const std::string& type) {
    errors_total_.Add({{ "type", type.empty() ? "unknown" : type }}).Increment();
}
