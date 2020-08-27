#pragma once

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mission/mission.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

using namespace mavsdk;

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
    std::shared_ptr<Telemetry> telemetry;
    Telemetry::Position  home;
    Telemetry::Health    health;
    
public:
    MissionEx(System &s):Mission(s){}
        
    
    /*
     double latMid, m_per_deg_lat, m_per_deg_lon, deltaLat, deltaLon,dist_m;

     latMid = (Lat1+Lat2 )/2.0;  // or just use Lat1 for slightly less accurate estimate


     m_per_deg_lat = 111132.954 - 559.822 * cos( 2.0 * latMid ) + 1.175 * cos( 4.0 * latMid);
     m_per_deg_lon = (3.14159265359/180 ) * 6367449 * cos ( latMid );

     deltaLat = fabs(Lat1 - Lat2);
     deltaLon = fabs(Lon1 - Lon2);

     dist_m = sqrt (  pow( deltaLat * m_per_deg_lat,2) + pow( deltaLon * m_per_deg_lon , 2) );
     */
    bool delta_latitude_longtitude_deg_to_north_east_meters( double lat1  , double lon1  ,
                                                             double lat2  , double lon2  ,
                                                             double &north, double &east )
    {
        double lat_mid       = (lat1+lat2) / 2.0;
        double m_per_deg_lat = 111132.954 - 559.822 * cos( 2.0 * lat_mid ) + 1.175 * cos( 4.0 * lat_mid);
        double m_per_deg_lon = (3.14159265359/180 ) * 6367449 * cos ( lat_mid );

        //std::cout << "delta alpha=" << m_per_deg_lat << ", beta=" << m_per_deg_lon << std::endl;
        //std::cout << "delta ll=" << (lat1 - lat2 ) << "," << (lon1 - lon2 ) << std::endl;
        
        north = (lat1 - lat2 ) * m_per_deg_lat;
        east  = (lon1 - lon2 ) * m_per_deg_lon;
        
        return true;
    }
    
    bool add_latitude_longtitude_deg_north_east_meters( double lat1  , double  lon1  ,
                                                        double north , double  east  ,
                                                        double &lat2 , double &lon2  )
    {
        double m_per_deg_lat = 111132.954 - 559.822 * cos( 2.0 * lat1 ) + 1.175 * cos( 4.0 * lat1 );
        double m_per_deg_lon = (3.14159265359/180 ) * 6367449 * cos ( lat1 );

        // std::cout << "add alpha=" << m_per_deg_lat << ", beta=" << m_per_deg_lon << std::endl;
        
        double delta_lat = north / m_per_deg_lat;
        double delta_lon = east  / m_per_deg_lon;
        
       // std::cout << "delta lat=" << delta_lat << "," << delta_lon << std::endl;
        lat2 = lat1 + delta_lat;
        lon2 = lon1 + delta_lon;
        
        // @todo - recalc again with lat_mid?
        return true;
    }
    
    bool result( const Mission::Result result, const char *title)
    {
        if (result != Mission::Result::Success) {
            std::cout << "Mission " << title << " failed (" << result << ")" << std::endl;
            return false;
        }
        std::cout << "Mission " << title << "ed" << std::endl;
        return true;
    }
    
    bool setup( std::shared_ptr<Telemetry> telemetry ){
        this->telemetry = telemetry;
        this->health    = this->telemetry->health();
        bool ok = this->health.is_home_position_ok;
        
        if ( ok ){
            this->home = this->telemetry->home();
            std::cout << "Home position " << this->home << std::endl;
        }
        else{
            std::cout << "Home position unknown!" << std::endl;
        }
        
        return ok;
    }
    
    bool setup( std::vector<MissionItemEx> &mi )
    {
        bool ok = true;
        mp.mission_items.clear();
        
        std::cout << "home @ " << this->home.latitude_deg << "," << this->home.longitude_deg << std::endl;
        
        for (auto it = begin (mi); it != end (mi); ++it) {
            Mission::MissionItem mi;
            mi = static_cast<Mission::MissionItem>(*it);
            
            double north = mi.latitude_deg;
            double east  = mi.longitude_deg;
            
            add_latitude_longtitude_deg_north_east_meters( this->home.latitude_deg, this->home.longitude_deg,
                                                                      north       ,            east         ,
                                                                      mi.latitude_deg, mi.longitude_deg     );
            
            std::cout << "waypoint (" << east << "," << north << ") => "
                                      << mi.latitude_deg << "," << mi.longitude_deg << std::endl;
            
            this->mp.mission_items.push_back( mi );
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

