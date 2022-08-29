//#include <rpc.h> //war fuer die uuid
#include <ctime>
#include "motis/core/common/unixtime.h"
#include "motis/core/common/logging.h"
#include "motis/core/common/constants.h"
#include "net/http/client/request.h"
#include "net/http/client/response.h"

#include "ctx/call.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "motis/json/json.h"

#include "motis/intermodal/ondemand_availability.h"

#include "motis/module/context/motis_http_req.h"

using namespace std;
using namespace motis::module;
using namespace motis::json;
using namespace net::http::client;
using namespace ctx;
using namespace rapidjson;

namespace motis::intermodal {
#define DELAY 900  // 15min

bool check_area(vector<Dot> polygon_area, Dot testdot) {
  bool res = false;
  int poly_size = static_cast<int>(polygon_area.size());
  if (poly_size < 3) {
    return res;
  }
  Dot infinity_dot;
  infinity_dot.lat = testdot.lat;
  infinity_dot.lng = 181;
  int result = 0;
  int j = 1;
  auto alignment = [&](Dot const* point, Dot const* dot,
                       Dot const* third) -> int {
    double val = (dot->lat - point->lat) * (third->lng - dot->lng) -
                 (dot->lng - point->lng) * (third->lat - dot->lat);
    if (val == 0.0)  // epsilon ?
    {
      // collinear
      return 0;
    }
    // 1 - below || 2 - above
    return (val > 0.0) ? 1 : 2;
  };
  auto onLine = [&](Dot const* point, Dot const* dot, Dot const* third) -> int {
    if (dot->lng <= max(point->lng, third->lng) &&
        dot->lng >= min(point->lng, third->lng) &&
        dot->lat <= max(point->lat, third->lat) &&
        dot->lat >= min(point->lat, third->lat))
      return true;
    return false;
  };
  auto find_intersection = [&](Dot const* point_one, Dot const* point_two,
                               Dot const* test_point,
                               Dot const* infinity_dot) -> bool {
    int orientation1 = alignment(point_one, point_two, test_point);
    int orientation2 = alignment(point_one, point_two, infinity_dot);
    int orientation3 = alignment(test_point, infinity_dot, point_one);
    int orientation4 = alignment(test_point, infinity_dot, point_two);
    if (orientation1 != orientation2 && orientation3 != orientation4)
      return true;
    if (orientation1 == 0 && onLine(point_one, test_point, point_two))
      return true;
    if (orientation2 == 0 && onLine(point_one, infinity_dot, point_two))
      return true;
    if (orientation3 == 0 && onLine(test_point, point_one, infinity_dot))
      return true;
    if (orientation4 == 0 && onLine(test_point, point_two, infinity_dot))
      return true;
    return false;
  };
  for (int i = 0; i < poly_size; i++) {
    if (find_intersection(&polygon_area[i], &polygon_area[j], &testdot,
                          &infinity_dot)) {
      if (alignment(&polygon_area[i], &testdot, &polygon_area[j]) == 0) {
        res = onLine(&polygon_area[i], &testdot, &polygon_area[j]);
        return res;
      }
      result++;
    }
    j++;
    if (j == poly_size) j = 0;
  }
  res = result % 2 == 1;
  return res;
}

availability_response read_result(const response& result, bool first, vector<Dot> dots)
{
  printf("read_result\n");
  availability_response ares;
  if(result.body.empty())
  {
    printf("Result Body Empty!\n");
    ares.available = false;
    return ares;
  }
  else
  {
    Document docu;
    if (docu.Parse(result.body.c_str()).HasParseError())
    {
      docu.GetParseError();
      throw utl::fail("On-Demand Availability Check Response: Bad JSON: {} at offset {}",
                      GetParseError_En(docu.GetParseError()),
                      docu.GetErrorOffset());
    }
    auto const& data = get_obj(docu, "data");
    auto read_jay_key_string = [&](char const* key, char const* name) -> string
    {
      auto const it = data.FindMember(key);
      if (it != data.MemberEnd() && it->value.IsString())
      {
        return it->value.GetString();
      }
      else if(it != data.MemberEnd() && it->value.HasMember(name))
      {
        auto const at = it->value.FindMember(name);
        if(at->value.IsString())
        {
          return at->value.GetString();
        }
      }
      return "";
    };
    auto read_jay_key_int = [&](char const* key, char const* name) -> int
    {
      auto const it = data.FindMember(key);
      if (it != data.MemberEnd() && it->value.IsInt())
      {
        return it->value.GetInt();
      }
      else if(it != data.MemberEnd() && it->value.HasMember(name))
      {
        auto const at = it->value.FindMember(name);
        if(at->value.IsInt())
        {
          return at->value.GetInt();
        }
      }
      return -1;
    };
    auto read_jay_key_double = [&](char const* key, char const* name) -> double
    {
      auto const it = data.FindMember(key);
      if (it != data.MemberEnd() && it->value.IsDouble())
      {
        return it->value.GetDouble();
      }
      else if(it != data.MemberEnd() && it->value.HasMember(name))
      {
        auto const at = it->value.FindMember(name);
        if(at->value.IsDouble())
        {
          return at->value.GetDouble();
        }
      }
      return -1.0;
    };
    auto read_jay_key_array = [&](char const* key, char const* name) -> vector<vector<double>>
    {
      auto const it = data.FindMember(key);
      vector<vector<double>> vec;
      if (it != data.MemberEnd() && it->value.IsObject())
      {
        auto const ar = it->value.FindMember(name);
        //Gesamtarray
        //ar->value[0].GetArray()[0]
        //ar->value[0].GetArray()[0].Size()
        //erstes Koordinatenarray
        //ar->value[0].GetArray()[0].GetArray()[0]
        //Inhalt des ersten Koordinatenarrays (Koordinate 0)
        //ar->value[0].GetArray()[0].GetArray()[0].GetArray()[0]
        vec.resize(ar->value[0].GetArray()[0].Size());
        for(SizeType k = 0; k < ar->value[0].GetArray()[0].Size(); k++)
        {
          const rapidjson::Value &data_vec = ar->value[0].GetArray()[0].GetArray()[k];
          for(SizeType j = 0; j < data_vec.Size(); j++)
            vec[k].push_back(data_vec[j].GetDouble());
        }
        return vec;
      }
      return vec;
    };

    if(first)
    {
      ares.codenumber_id = read_jay_key_string("id", " ");
      vector<vector<double>> polypoints = read_jay_key_array("area", "coordinates");
      vector<Dot> polygon_area;
      polygon_area.resize(polypoints.size());
      int k = 0;
      for(vector<double> vec : polypoints)
      {
        if(vec.size() == 2)
        {
          polygon_area[k].lat = vec[0];
          polygon_area[k].lng = vec[1];
        }
        else
        {
          LOG(logging::warn) << "invalid number of coordinates. "
                                "In http (get) result, with id: "
                             << ares.codenumber_id << "!"
                             << "availability is set to false";
          ares.available = false;
          return ares;
        }
        k++;
      }
      bool inside_start = check_area(polygon_area, dots.at(0));
      bool inside_end = check_area(polygon_area, dots.at(1));
      ares.available = inside_start && inside_end;
      return ares;
    }
    else
    {
      ares.codenumber_id = read_jay_key_string("id", " ");
      ares.startpoint.lat  = read_jay_key_double("pickup", "lat");
      ares.startpoint.lng = read_jay_key_double("pickup", "lng");
      ares.endpoint.lat = read_jay_key_double("dropoff", "lat");
      ares.endpoint.lng = read_jay_key_double("dropoff", "lng");
      ares.price = read_jay_key_double("fare", "final_price");
      ares.walkDur[0] = read_jay_key_int("pickup", "walking_duration");
      ares.walkDur[1] = read_jay_key_int("dropoff", "walking_duration");
      //string pu_time1 = read_jay_key_string("pickup", "time");
      string pu_time2 = read_jay_key_string("pickup", "negotiation_time");
      //string pu_time3 = read_jay_key_string("pickup", "negotiation_time_max");
      //string do_time1 = read_jay_key_string("dropoff", "time");
      string do_time2 = read_jay_key_string("dropoff", "negotiation_time");
      //string do_time3 = read_jay_key_string("dropoff", "negotiation_time_max");
      //"2017-09-06T15:13:43Z" -> 1504703623
      auto traveltime_to_unixtime = [&](const string& timestring) -> int64_t
      {
        int year = stoi(timestring.substr(0, 4));
        int month = stoi(timestring.substr(5, 2));
        int day = stoi(timestring.substr(8, 2));
        size_t pos_T = timestring.find('T');
        string str = timestring.substr(pos_T + 1, 8);
        str.erase(2,1);
        str.erase(4,1);
        int hour = stoi(str.substr(0, 2));
        int min = stoi(str.substr(2, 2));
        int sec = stoi(str.substr(4, 2));
        boost::gregorian::date firstswitchday(year, month, day);
        boost::gregorian::date secondswitchday(year, month, day);
        for(int i = 25; i < 32; i++)
        {
          boost::gregorian::date::year_type y = year;
          boost::gregorian::date::day_type d = i;
          boost::gregorian::date date(y, 3, d);
          if(0 == date.day_of_week()) // 0 Sonntag ?
          {
            firstswitchday = {y, 3, d};
          }
          boost::gregorian::date date2(y, 10, d);
          if(0 == date2.day_of_week())
          {
            secondswitchday = {y, 10, d};
          }
        }
        boost::gregorian::date currentday(year, month, day);
        struct tm name = {0};
        name.tm_year = year - 1900;
        name.tm_mon = month - 1;
        name.tm_mday = day;
        name.tm_hour = hour;
        name.tm_min = min;
        name.tm_sec = sec;
        if(currentday >= firstswitchday && currentday <= secondswitchday && hour > 2)
        {
          // march until october
          name.tm_isdst = 1;
        }
        else if(currentday >= secondswitchday && currentday <= firstswitchday && hour > 2)
        {
          // october until march
          name.tm_isdst = 0;
        }
        time_t result = mktime(&name);
        return result;
      };
      //euros.arrivalTime[0] = traveltime_to_unixtime(pu_time1);
      ares.pickupTime[1] = traveltime_to_unixtime(pu_time2);
      //euros.arrivalTime[2] = traveltime_to_unixtime(pu_time1);
      //euros.departureTime[0] = traveltime_to_unixtime(do_time1);
      ares.dropoffTime[1] = traveltime_to_unixtime(do_time2);
      //euros.departureTime[2] = traveltime_to_unixtime(do_time3);
      return ares;
    }
  }
}

string create_json_body(const availability_request& areq)
{
  // Creates a Ride Inquiry object with estimations and availability information - POST api/passenger/ride_inquiry IOKI
  string json = R"( {
                      "data": {
                      "product_id": ")" + areq.productID + "\","
                + R"( "origin": {
                        "lat": )" + to_string(areq.startpoint.lat) + ","
                + R"( "lng": )" + to_string(areq.startpoint.lng) + ","
                + R"(  "location_name": "",
                         "street_name": "",
                         "street_number": "",
                         "postal_code": "",
                         "city": "",
                         "county": "",
                         "country": "Germany",
                         "time": )" + to_string(areq.departureTime)
                + R"( "station_id": "string" + "},"
                      + "destination": {
                        "lat": )" + to_string(areq.endpoint.lat) + ","
                + R"( "lng": )" + to_string(areq.endpoint.lng) + ","
                + R"(  "location_name": "string",
                         "street_name": "string",
                         "street_number": "string",
                         "postal_code": "string",
                         "city": "string",
                         "county": "string",
                         "country": "string",
                         "station_id": ""
                         "time": )" + to_string(areq.arrivalTime_onnext)
                + "}}}";
  /*
   + "," + R"( "maxWalkDistance": )" + to_string(mars.maxWalkDist)
   + "}}";*/
  return json;
}

// schauen ob man das vereinfachen kann...
bool checking(const availability_request& areq, const availability_response& ares)
{
  auto coord_equality = [&](const string& eins, const string& zwei) -> bool
  {
    size_t lastidx1 = eins.length() - 1;
    size_t lastidx2 = zwei.length() - 1;
    size_t pidx1 = eins.find('.');
    size_t pidx2 = zwei.find('.');
    string one = eins.substr(0, lastidx1);
    string oneverify = one.substr(pidx1 + 1, one.length() - 1 - pidx1);
    string two = zwei.substr(0, lastidx2);
    string twoverify = two.substr(pidx2 + 1, two.length() - 1 - pidx2);
    if(oneverify.length() != twoverify.length())
    {
      if(oneverify.length() > twoverify.length())
      {
        if(twoverify.length() < 4) return false;
        size_t diff = oneverify.length() - twoverify.length();
        string newone = one.substr(one.length() - 1 - diff, diff);
        return newone == two;
      }
      else
      {
        if(oneverify.length() < 4) return false;
        size_t diff = twoverify.length() - oneverify.length();
        string newtwo = two.substr(two.length() - 1 - diff, diff);
        return newtwo == one;
      }
    }
    else
    {
      return one == two;
    }
  };
  bool coord_start, coord_end, walklength, walktime, timewindow, waiting;
  bool result;
  if(areq.start)
  {
    waiting = areq.departureTime + DELAY > ares.pickupTime[1] && // 1300 +15 = 1315 > 1310
              areq.departureTime - DELAY < ares.pickupTime[1]; // 1300 -15 = 1245 < 1310
    //printf("waiting: %lld + 15 > %lld \n -- %lld - 15 < %lld\n", areq.departureTime, ares.pickupTime[1], areq.departureTime, ares.pickupTime[1]);
    //printf("duration: %lld \n response duration complete: %lld \n", areq.duration,
    // (ares.dropoffTime[1] - ares.pickupTime[1]) + ares.walkDur[0] + ares.walkDur[1]);
    if(ares.walkDur[0] == 0 && ares.walkDur[1] == 0)
    {
      coord_start = coord_equality(to_string(areq.startpoint.lat), to_string(ares.startpoint.lat))
                    && coord_equality(to_string(areq.startpoint.lng), to_string(ares.startpoint.lng));
      coord_end = coord_equality(to_string(areq.endpoint.lat), to_string(ares.endpoint.lat)) &&
                  coord_equality(to_string(areq.endpoint.lng), to_string(ares.endpoint.lng));
      timewindow = areq.duration >= ares.dropoffTime[1] - ares.pickupTime[1];
      //printf("checking start: waiting: %d; coord_start: %d; coord_end: %d; timewindow: %d\n", waiting, coord_start, coord_end, timewindow);
      result = coord_start && coord_end && waiting && timewindow;
    }
    else if(ares.walkDur[0] != 0 && ares.walkDur[1] == 0)
    {
      walktime = ares.walkDur[0] < MAX_WALK_TIME;
      walklength = areq.maxWalkDist >= ares.walkDur[0] * WALK_SPEED;
      timewindow = areq.duration >= (ares.dropoffTime[1] - ares.pickupTime[1]) + ares.walkDur[0];
      //printf("checking: waiting: %d; walktime: %d; walklength: %d; timewindow: %d\n", waiting, walktime, walklength, timewindow);
      result = walklength && walktime && waiting && timewindow;
    }
    else if(ares.walkDur[1] != 0 && ares.walkDur[0] == 0)
    {
      walktime = ares.walkDur[1] < MAX_WALK_TIME &&
                 ares.dropoffTime[1] + ares.walkDur[1] < areq.arrivalTime_onnext; // 1350 +5 = 1355 < 1400
      walklength = areq.maxWalkDist >= ares.walkDur[1] * WALK_SPEED;
      timewindow = areq.duration >= (ares.dropoffTime[1] - ares.pickupTime[1]) + ares.walkDur[1];
      //printf("checking: waiting: %d; walktime: %d; walklength: %d; timewindow: %d\n", waiting, walktime, walklength, timewindow);
      result = walklength && walktime && waiting && timewindow;
    }
    else
    {
      walktime = ares.walkDur[0] < MAX_WALK_TIME && ares.walkDur[1] < MAX_WALK_TIME
                 && ares.dropoffTime[1] + ares.walkDur[1] < areq.arrivalTime_onnext;
      walklength = areq.maxWalkDist >= ares.walkDur[1] * WALK_SPEED && areq.maxWalkDist >= ares.walkDur[0] * WALK_SPEED;
      timewindow = areq.duration >= (ares.dropoffTime[1] - ares.pickupTime[1]) + ares.walkDur[0] + ares.walkDur[1];
      //printf("checking: waiting: %d; walktime: %d; walklength: %d; timewindow: %d\n", waiting, walktime, walklength, timewindow);
      result = walklength && walktime && waiting && timewindow;
    }
  }
  else
  {
    waiting = areq.departureTime + DELAY > ares.pickupTime[1] // 1300 +15 = 1315 > 1310
              && areq.arrivalTime < ares.pickupTime[1];       // 1258 < 1310
    //printf("waiting: %lld + 15 > %lld \n -- %lld < %lld\n", areq.departureTime, ares.pickupTime[1], areq.arrivalTime, ares.pickupTime[1]);
    if(ares.walkDur[0] == 0 && ares.walkDur[1] == 0)
    {
      coord_start = coord_equality(to_string(areq.startpoint.lat), to_string(ares.startpoint.lat)) &&
                    coord_equality(to_string(areq.startpoint.lng), to_string(ares.startpoint.lng));
      coord_end = coord_equality(to_string(areq.endpoint.lat), to_string(ares.endpoint.lat)) &&
                  coord_equality(to_string(areq.endpoint.lng), to_string(ares.endpoint.lng));
      timewindow = areq.duration >= ares.dropoffTime[1] - ares.pickupTime[1];
      //printf("checking end: waiting: %d; coord_start: %d; coord_end: %d; timewindow: %d\n", waiting, coord_start, coord_end, timewindow);
      result = coord_start && coord_end && waiting && timewindow;
    }
    else if(ares.walkDur[0] != 0 && ares.walkDur[1] == 0)
    {
      walktime = areq.departureTime + ares.walkDur[0] < ares.pickupTime[1] && ares.walkDur[0] < MAX_WALK_TIME; // 1300 +5 = 1305 < 1310
      walklength = areq.maxWalkDist >= ares.walkDur[0] * WALK_SPEED;
      timewindow = areq.duration >= (ares.dropoffTime[1] - ares.pickupTime[1]) + ares.walkDur[0];
      //printf("checking: waiting: %d; walktime: %d; walklength: %d; timewindow: %d\n", waiting, walktime, walklength, timewindow);
      result = walklength && walktime && waiting && timewindow;
    }
    else if(ares.walkDur[1] != 0 && ares.walkDur[0] == 0)
    {
      walktime = ares.walkDur[1] < MAX_WALK_TIME;
      walklength = areq.maxWalkDist >= ares.walkDur[1] * WALK_SPEED;
      timewindow = areq.duration >= (ares.dropoffTime[1] - ares.pickupTime[1]) + ares.walkDur[1]; // evtl egal -> dann länge der kante anpassen ?
      //printf("checking: waiting: %d; walktime: %d; walklength: %d; timewindow: %d\n", waiting, walktime, walklength, timewindow);
      result = walklength && walktime && waiting && timewindow;
    }
    else
    {
      walktime = ares.walkDur[0] < MAX_WALK_TIME && ares.walkDur[1] < MAX_WALK_TIME
                 && areq.departureTime + ares.walkDur[0] < ares.pickupTime[1];
      walklength = areq.maxWalkDist >= ares.walkDur[1] * WALK_SPEED && areq.maxWalkDist >= ares.walkDur[0] * WALK_SPEED;
      timewindow = areq.duration >= (ares.dropoffTime[1] - ares.pickupTime[1]) + ares.walkDur[0] + ares.walkDur[1];
      //printf("checking: waiting: %d; walktime: %d; walklength: %d; timewindow: %d\n", waiting, walktime, walklength, timewindow);
      result = walklength && walktime && waiting && timewindow;
    }
  }
  return result;
}

availability_response check_od_availability(const availability_request areq)
{
  printf("check_od_availability!\n");
  string addr = "http://127.0.0.1:9000/"; //TESTSERVER
  request::method m = request::GET;
  map<string, string> hdrs;
  hdrs.insert(pair<string, string>("Accept","application/json"));
  request req(addr, m, hdrs, "");
  Dot req_dot_start;
  Dot req_dot_end;
  req_dot_end.lat = areq.endpoint.lat;
  req_dot_end.lng = areq.endpoint.lng;
  req_dot_start.lat = areq.startpoint.lat;
  req_dot_start.lng = areq.startpoint.lng;
  vector<Dot> req_dots;
  req_dots.emplace_back(req_dot_start);
  req_dots.emplace_back(req_dot_end);
  response firstresult = motis_http(req)->val();
  availability_response response_first = read_result(firstresult, true, req_dots);
  if(!response_first.available)
  {
    return response_first;
  }
  else
  {
    request::method m2 = request::POST;
    map<string, string> hdrs2;
    //UUID uuid;
    //UuidCreate(&uuid);
    //char* random_uuid_str;
    //UuidToStringA(&uuid, (RPC_CSTR*)&random_uuid_str);
    //hdrs2.insert(pair<string, string>("Idempotency-Key", random_uuid_str));
    hdrs2.insert(pair<string, string>("Accept","application/json"));
    string body = create_json_body(areq);
    request req2(addr); //Testserver
    req2.req_method = m2;
    req2.headers = hdrs2;
    req2.body = body;
    response secondresult = motis_http(req2)->val();
    availability_response response_second = read_result(secondresult, false, req_dots);
    response_second.available = checking(areq, response_second);
    return response_second;
  }
}

} // namespace intermodal