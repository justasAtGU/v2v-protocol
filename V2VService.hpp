#ifndef V2V_PROTOCOL_DEMO_V2VSERVICE_HPP
#define V2V_PROTOCOL_DEMO_V2VSERVICE_HPP

#include <iomanip>
#include <unistd.h>
#include <sys/time.h>
#include "cluon/OD4Session.hpp"
#include "cluon/UDPSender.hpp"
#include "cluon/UDPReceiver.hpp"
#include "cluon/Envelope.hpp"
#include "Messages.hpp"
#include <iostream>
#include <chrono>
#include <thread>


/********************************************************/
/** Car constants ***************************************/
/********************************************************/
static const std::string DASH_ID  = "1";
static std::string DASH_IP;
static float FREQ;
static int TIME_DIFF;
std::string GROUP_ID;
std::shared_ptr<cluon::OD4Session>  internal;


/********************************************************/
/** Message constants ***************************************/
/********************************************************/
static float PEDAL_SPEED;
static float STEERING_ANGLE;


/********************************************************/
/** V2V Protocol specific constants *********************/
/********************************************************/
static const int BROADCAST_CHANNEL 	= 250;
static const int DEFAULT_PORT 		= 50001;

static const int ANNOUNCE_PRESENCE	= 1001;
static const int FOLLOW_REQUEST 	= 1002;
static const int FOLLOW_RESPONSE 	= 1003;
static const int STOP_FOLLOW 		= 1004;
static const int LEADER_STATUS 		= 2001;
static const int FOLLOWER_STATUS 	= 3001;


/********************************************************/
/** Dash specific communication constants ***************/
/********************************************************/
static const int INTERNAL_CHANNEL 	= 122;

static const int PEDAL_POSITION 	= 1041;
static const int GROUND_STEERING	= 1045;
static const int ULTRASONIC_FRONT 	= 2201; 
static const int IMU 				= 2202;
static const int LEADER_ID          = 2204;
static const int KILL_SWITCH        = 2205;


class V2VService {
public:
    std::map <std::string, std::string> presentCars;
    std::string leaderIp;
    std::string followerIp;

    V2VService();

    void announcePresence();
    void followRequest(std::string vehicleIp);
    void followResponse();
    void stopFollow(std::string vehicleIp);
    void leaderStatus();
    void followerStatus();
    bool carConnectionLost(int request);

private:
    time_t leaderFreq;
    time_t followerFreq;

    /** OD4 Sessions *****************************/
    std::shared_ptr<cluon::OD4Session>  broadcast;

    /** UDP Connections **************************/
    std::shared_ptr<cluon::UDPReceiver> incoming;
    std::shared_ptr<cluon::UDPSender>   toLeader;
    std::shared_ptr<cluon::UDPSender>   toFollower;

    static uint64_t getTime();
    static std::pair<int16_t, std::string> extract(std::string data);
    template <class T>
    static std::string encode(T msg);
    template <class T>
    static T decode(std::string data);
};

#endif //V2V_PROTOCOL_DEMO_V2VSERVICE_HPP
