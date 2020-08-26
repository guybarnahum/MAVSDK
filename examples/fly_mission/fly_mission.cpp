/**
 * @file fly_mission.cpp
 *
 * @brief Demonstrates how to Add & Fly Waypoint missions using the MAVSDK.
 * The example is summarised below:
 * 1. Adds mission items.
 * 2. Starts mission from first mission item.
 * 3. Illustrates Pause/Resume mission item.
 * 4. Exits after the mission is accomplished.
 *
 * @author Julian Oes <julian@oes.ch>,
 *         Shakthi Prashanth M <shakthi.prashanth.m@intel.com>
 * @date 2017-09-06
 */

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/mission/mission.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

#include <functional>
#include <future>
#include <iostream>

#define ERROR_CONSOLE_TEXT "\033[31m" // Turn text on console red
#define TELEMETRY_CONSOLE_TEXT "\033[34m" // Turn text on console blue
#define NORMAL_CONSOLE_TEXT "\033[0m" // Restore normal console colour

using namespace mavsdk;
using namespace std::placeholders; // for `_1`
using namespace std::chrono; // for seconds(), milliseconds()
using namespace std::this_thread; // for sleep_for()

// Handles Action's result
inline void handle_action_err_exit(Action::Result result, const std::string& message);
// Handles Mission's result
inline void handle_mission_err_exit(Mission::Result result, const std::string& message);
// Handles Connection result
inline void handle_connection_err_exit(ConnectionResult result, const std::string& message);

// Mission::MissionItem lacks a constructor - extend class
struct MissionItemEx : public Mission::MissionItem{
    
public:
    MissionItemEx( double latitude_deg,
                   double longitude_deg,
                   float relative_altitude_m,
                   float speed_m_s,
                   bool is_fly_through,
                   float gimbal_pitch_deg,
                   float gimbal_yaw_deg,
                   Mission::MissionItem::CameraAction camera_action)
    {
        this->latitude_deg        = latitude_deg;
        this->longitude_deg       = longitude_deg;
        this->relative_altitude_m = relative_altitude_m;
        this->speed_m_s           = speed_m_s;
        this->is_fly_through      = is_fly_through;
        this->gimbal_pitch_deg    = gimbal_pitch_deg;
        this->gimbal_yaw_deg      = gimbal_yaw_deg;
        this->camera_action       = camera_action;
    }
};

// Wrapper on mavsdk::Mission
class MissionEx : public Mission{
    
private:
    Mission::MissionPlan mp;
    
public:
    MissionEx(System &s):Mission(s){}
        
    bool result( const Mission::Result result, const char *title)
    {
        if (result != Mission::Result::Success) {
            std::cout << "Mission " << title << " failed (" << result << ")" << std::endl;
            return false;
        }
        std::cout << "Mission " << title << "ed" << std::endl;
        return true;
    }
    
    bool setup( std::vector<MissionItemEx> &mi )
    {
        bool ok = true;
        mp.mission_items.clear();
        for (auto it = begin (mi); it != end (mi); ++it) {
            this->mp.mission_items.push_back( static_cast<Mission::MissionItem>(*it) );
        }
        return ok;
    }
    
    bool clear (const char *title = "clear"){
        std::cout << "Mission " << title << std::endl;
        return this->result( this->clear_mission() , title );
    }
    
    bool upload(const char *title = "upload"){
        std::cout << "Mission " << title << " (" << this->mp.mission_items.size() << ")" << std::endl;
        auto prom = std::make_shared<std::promise<Mission::Result>>();
        auto future_result = prom->get_future();
        this->upload_mission_async( this->mp, [prom](Mission::Result result) { prom->set_value(result); });
        return this->result( future_result.get() , title  );
    }
    
    bool pause (const char *title = "pause"){
        std::cout << "Mission " << title << std::endl;
        auto prom = std::make_shared<std::promise<Mission::Result>>();
        auto future_result = prom->get_future();
        this->pause_mission_async( [prom](Mission::Result result) { prom->set_value(result); });
        return this->result( future_result.get() , title );
    }
        
    bool start (const char *title = "start"){
        std::cout << "Mission " << title << std::endl;
        auto prom = std::make_shared<std::promise<Mission::Result>>();
        auto future_result = prom->get_future();
        this->start_mission_async( [prom](Mission::Result result){ prom->set_value(result); });
        return this->result( future_result.get() , title );
    }
    
    bool resume(const char *title = "resume" ){
        return this->start(title);
    }
    
    bool stop  (){
        this->pause("stop");
        return this->clear();
    }
};

bool clear_mission ( std::shared_ptr<MissionEx> mission );
bool upload_mission( std::shared_ptr<MissionEx> mission , const std::vector<MissionItemEx> &mi );

std::vector<MissionItemEx> g_mi =
{
    {47.398170327054473,
        8.5456490218639658,
        10.0f,
        5.0f,
        false,
        20.0f,
        60.0f,
        Mission::MissionItem::CameraAction::None
    },
    { 47.398241338125118,8.5455360114574432,
        10.0f,
        2.0f,
        true,
        0.0f,
        -60.0f,
        Mission::MissionItem::CameraAction::TakePhoto
    },
    {
        47.398139363821485,
        8.5453846156597137,
        10.0f,
        5.0f,
        true,
        -45.0f,
        0.0f,
        Mission::MissionItem::CameraAction::StartVideo
    },
    {
        47.398058617228855,
        8.5454618036746979,
        10.0f,
        2.0f,
        false,
        -90.0f,
        30.0f,
        Mission::MissionItem::CameraAction::StopVideo
    },
    {
        47.398100366082858,
        8.5456969141960144,
        10.0f,
        5.0f,
        false,
        -45.0f,
        -30.0f,
        Mission::MissionItem::CameraAction::StartPhotoInterval
    },
    {
        47.398001890458097,
        8.5455576181411743,
        10.0f,
        5.0f,
        false,
        0.0f,
        0.0f,
        Mission::MissionItem::CameraAction::StopPhotoInterval
    },
    {
        47.398170327054473,
        8.5456490218639658,
        10.0f,
        5.0f,
        false,
        20.0f,
        60.0f,
        Mission::MissionItem::CameraAction::None
    }
};

void usage(std::string bin_name)
{
    std::cout << NORMAL_CONSOLE_TEXT << "Usage : " << bin_name << " <connection_url>" << std::endl
              << "Connection URL format should be :" << std::endl
              << " For TCP : tcp://[server_host][:server_port]" << std::endl
              << " For UDP : udp://[bind_host][:bind_port]" << std::endl
              << " For Serial : serial:///path/to/serial/dev[:baudrate]" << std::endl
              << "For example, to connect to the simulator use URL: udp://:14540" << std::endl;
}

int main(int argc, char** argv)
{
    Mavsdk dc;

    {
        auto prom = std::make_shared<std::promise<void>>();
        auto future_result = prom->get_future();

        std::cout << "Waiting to discover system..." << std::endl;
        dc.register_on_discover([prom](uint64_t uuid) {
            std::cout << "Discovered system with UUID: " << uuid << std::endl;
            prom->set_value();
        });

        std::string connection_url;
        ConnectionResult connection_result;

        if (argc == 2) {
            connection_url = argv[1];
            connection_result = dc.add_any_connection(connection_url);
        } else {
            usage(argv[0]);
            return 1;
        }

        if (connection_result != ConnectionResult::Success) {
            std::cout << ERROR_CONSOLE_TEXT << "Connection failed: " << connection_result
                      << NORMAL_CONSOLE_TEXT << std::endl;
            return 1;
        }

        future_result.get();
    }

    dc.register_on_timeout([](uint64_t uuid) {
        std::cout << "System with UUID timed out: " << uuid << std::endl;
        std::cout << "Exiting." << std::endl;
        exit(0);
    });

    // We don't need to specifiy the UUID if it's only one system anyway.
    // If there were multiple, we could specify it with:
    // dc.system(uint64_t uuid);
    System& system = dc.system();
    auto action = std::make_shared<Action>(system);
    auto mission = std::make_shared<MissionEx>(system);
    auto telemetry = std::make_shared<Telemetry>(system);

    while (!telemetry->health_all_ok()) {
        std::cout << "Waiting for system to be ready" << std::endl;
        sleep_for(seconds(1));
    }

    std::cout << "System ready" << std::endl;
    
    mission->clear();
    mission->setup( g_mi );
    mission->upload();
    
    std::cout << "Arming..." << std::endl;
    const Action::Result arm_result = action->arm();
    handle_action_err_exit(arm_result, "Arm failed: ");
    std::cout << "Armed." << std::endl;

    std::atomic<bool> want_to_pause{false};
    // Before starting the mission, we want to be sure to subscribe to the mission progress.
    mission->subscribe_mission_progress(
        [&want_to_pause](Mission::MissionProgress mission_progress) {
            std::cout << "Mission status update: " << mission_progress.current << " / "
                      << mission_progress.total << std::endl;

            if (mission_progress.current >= 2) {
                // We can only set a flag here. If we do more request inside the callback,
                // we risk blocking the system.
                want_to_pause = true;
            }
        });

    mission->start();

    while (!want_to_pause) {
        sleep_for(seconds(1));
    }

    mission->pause();

    // Pause for 5 seconds.
    sleep_for(seconds(5));

    // Then continue.
    mission->resume();

    while (!mission->is_mission_finished().second) {
        sleep_for(seconds(1));
    }

    mission->clear();
    
    {
        // We are done, and can do RTL to go home.
        std::cout << "Commanding RTL..." << std::endl;
        const Action::Result result = action->return_to_launch();
        if (result != Action::Result::Success) {
            std::cout << "Failed to command RTL (" << result << ")" << std::endl;
        } else {
            std::cout << "Commanded RTL." << std::endl;
        }
    }

    // We need to wait a bit, otherwise the armed state might not be correct yet.
    sleep_for(seconds(2));

    while (telemetry->armed()) {
        // Wait until we're done.
        sleep_for(seconds(1));
    }
    std::cout << "Disarmed, exiting." << std::endl;
}

bool upload_mission( std::shared_ptr<MissionEx> mission, const std::vector<MissionItemEx> &mi )
{
    bool ok = true;

    std::cout << "Uploading mission... (" << mi.size() << ")" << std::endl;
    // We only have the upload_mission function asynchronous for now, so we wrap it using
    // std::future.
    auto prom = std::make_shared<std::promise<Mission::Result>>();
    auto future_result = prom->get_future();
    Mission::MissionPlan mission_plan{};
    
    mission_plan.mission_items.clear();
    
    // c++ can't cast vector of extended classes to base class
    for (auto it = begin (mi); it != end (mi); ++it) {
        mission_plan.mission_items.push_back( static_cast<Mission::MissionItem>(*it) );
    }
    
    mission->upload_mission_async(
        mission_plan, [prom](Mission::Result result) { prom->set_value(result); }
        );

    const Mission::Result result = future_result.get();
    if (result != Mission::Result::Success) {
        std::cout << "Mission upload failed (" << result << "), exiting." << std::endl;
        return !ok;
    }
    std::cout << "Mission uploaded." << std::endl;
    return ok;
}

inline void handle_action_err_exit(Action::Result result, const std::string& message)
{
    if (result != Action::Result::Success) {
        std::cerr << ERROR_CONSOLE_TEXT << message << result << NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}

inline void handle_mission_err_exit(Mission::Result result, const std::string& message)
{
    if (result != Mission::Result::Success) {
        std::cerr << ERROR_CONSOLE_TEXT << message << result << NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Handles connection result
inline void handle_connection_err_exit(ConnectionResult result, const std::string& message)
{
    if (result != ConnectionResult::Success) {
        std::cerr << ERROR_CONSOLE_TEXT << message << result << NORMAL_CONSOLE_TEXT << std::endl;
        exit(EXIT_FAILURE);
    }
}
