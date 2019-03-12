#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "MPC.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

int main() {
  uWS::Hub h;

  // MPC is initialized here!
  MPC mpc;

  h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    string sdata = string(data).substr(0, length);
    std::cout << sdata << std::endl;
    if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
      string s = hasData(sdata);
      if (s != "") {
        auto j = json::parse(s);
        string event = j[0].get<string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          vector<double> ptsx = j[1]["ptsx"];
          vector<double> ptsy = j[1]["ptsy"];
          double px = j[1]["x"];
          double py = j[1]["y"];
          double psi = j[1]["psi"];
          double v = j[1]["speed"];

          // Transforming wayspoints from global map into vehicles' own coordinate system
          // This is great for both visualization and for calculating CTE as per projects
          // tips and tricks section.
          // Without such normalization MPC behaves very unstable b/c optimized values are
          // not well conditioned.
          for (int i = 0; i < ptsx.size(); i++) {
            double delta_x = ptsx[i] - px;
            double delta_y = ptsy[i] - py;
            ptsx[i] = delta_x * cos(-psi) - delta_y * sin(-psi);
            ptsy[i] = delta_x * sin(-psi) + delta_y * cos(-psi);
          }

          // Converting into VectorXd format as that's the format accepted by polyfit function.
          VectorXd ptsx_ = Eigen::Map<Eigen::VectorXd, Eigen::Unaligned>(ptsx.data(), ptsx.size());
          VectorXd ptsy_ = Eigen::Map<Eigen::VectorXd, Eigen::Unaligned>(ptsy.data(), ptsy.size());

          auto coeffs = polyfit(ptsx_, ptsy_, 3);

          // Calculate CTE and PsiE
          // No subtraction by current's vehicle position in Y b/c we transformed into vehicles coord system.
          // Similarly, for epsi, our current direction is always at 0 degrees (b/c of the local coordinate system).
          auto cte = polyeval(coeffs, 0);
          auto epsi = -atan(coeffs[1]);

          VectorXd state(6);
          // State will be fed with now transformed coordinates in vehicle's coordinate system.
          state << 0, 0, 0, v, cte, epsi;
          auto vars = mpc.Solve(state, coeffs);
           
          // Multiplying steering value by -1 per tips and tricks in the project section.
          double steer_value = -vars[0];
          double throttle_value = vars[1];

          json msgJson;
          // Dividing the steering value by deg2rad(25) to keep it in range in between
          //   [-deg2rad(25), deg2rad(25] instead of [-1, 1].
          msgJson["steering_angle"] = steer_value / deg2rad(25);
          msgJson["throttle"] = throttle_value;

          // Display the MPC predicted trajectory 
          vector<double> mpc_x_vals;
          vector<double> mpc_y_vals;
          vector<double> next_x_vals = ptsx;
          vector<double> next_y_vals = ptsy;

          // All predicted waypoints are located at index 2 and above. X coordinate is always followed by corresponding Y.
          // Display the waypoints/reference line

          for (int i = 2 ; i < vars.size(); i + 2){
            mpc_x_vals.push_back(vars[i]);
            mpc_y_vals.push_back(vars[i + 1]);
          }


          msgJson["mpc_x"] = mpc_x_vals;
          msgJson["mpc_y"] = mpc_y_vals;

          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;


          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          std::cout << msg << std::endl;
          // Latency
          // The purpose is to mimic real driving conditions where
          //   the car does actuate the commands instantly.
          //
          // Feel free to play around with this value but should be to drive
          //   around the track with 100ms latency.
          //
          // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE SUBMITTING.

          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}
