#include <string>
#include <vector>
#include <map>
#include <mutex>

#include "motis/core/common/unixtime.h"
#include "geo/latlng.h"
#include "motis/intermodal/statistics.h"

namespace motis::intermodal {

struct availability_response {
  bool available_ = false;
  std::string codenumber_id_ = " ";
  geo::latlng startpoint_ = {-1.0, -1.0};
  geo::latlng endpoint_ = {-1.0, -1.0};
  unixtime pickup_time_ = -1;
  unixtime dropoff_time_ = -1;
  double price_ = -1.0;
  std::vector<int> walk_dur_ = {0, 0};  // start, ziel; in sekunden
  unixtime complete_duration_ = -1;
  int journey_id_ = -1;
};

struct availability_request {
  bool start_ = false;
  bool direct_con_ = false;
  std::string product_id_ = "string";
  int64_t duration_ = -1; // in sekunden
  geo::latlng startpoint_ = {-1.0, -1.0};
  geo::latlng endpoint_ = {-1.0, -1.0};
  unixtime departure_time_ = -1;
  unixtime arrival_time_ = -1;
  unixtime arrival_time_onnext_ = -1;
  int max_walk_dist_ = 500; // meter

  std::string product_check_addr_;
  std::map<std::string, std::string> hdrs_;
  int journey_id_ = -1;
};

void check_od_availability(const availability_request&, statistics&, std::vector<availability_response>&);
bool check_od_area(geo::latlng, geo::latlng, std::vector<std::string> const&, statistics&);

} //namespace motis::intermodal