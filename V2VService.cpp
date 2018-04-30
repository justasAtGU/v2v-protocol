#include "V2VService.hpp"

int main(int argc, char **argv) {
    std::shared_ptr<V2VService> v2vService = std::make_shared<V2VService>();

    // Getting command line arguments
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);

    // In case no IP, time diffrence or frequency is provided do nothing
    if (commandlineArguments.count("ip") == 0 || commandlineArguments.count("diff") == 0 
      || commandlineArguments.count("freq") == 0)
    {
        std::cerr << "You must specify your car's IP and the desired time to wait between status request" << std::endl;
        std::cerr << "Example: " << argv[0] << " --ip=192.168.8.1 --diff=2000 --freq=5" << std::endl;;
        return -1;
    }
    else
    {
      // Parse arguments
      DASH_IP = commandlineArguments["ip"];
      TIME_DIFF = stoi(commandlineArguments["diff"]);
      FREQ = stof(commandlineArguments["freq"]);
      
      // Default leader group number
      GROUP_ID = "7";

      // Internal communication OD4 session
      internal =
      std::make_shared<cluon::OD4Session>(INTERNAL_CHANNEL,
        [](cluon::data::Envelope &&envelope) noexcept {
          switch (envelope.dataType()) {
            case IMU: {
              readingsIMU imu = cluon::extractMessage<readingsIMU>(std::move(envelope));
              PEDAL_SPEED = imu.readingSpeed();
              STEERING_ANGLE = imu.readingSteeringAngle();
            } break;
            case PEDAL_POSITION: { 
              opendlv::proxy::PedalPositionReading p = cluon::extractMessage<opendlv::proxy::PedalPositionReading>(std::move(envelope));
              PEDAL_SPEED = p.percent();
            } break;
            case GROUND_STEERING: {
              opendlv::proxy::GroundSteeringReading g = cluon::extractMessage<opendlv::proxy::GroundSteeringReading>(std::move(envelope));
              STEERING_ANGLE = g.steeringAngle();
            } break;
            case LEADER_ID: {
              LeaderId id = cluon::extractMessage<LeaderId>(std::move(envelope));
              GROUP_ID = id.groupId();
            } break;
            default: break;
          }
      });

      /*
       * Method used to constantly send messages to a thread
       * AnnouncePresence automatically stops sending when there's an established connection
       * FollowerStatus/leaderStatus send to a specific IP, if the car is a leader it sends to the followerIP and vice versa
       * So the UDP connections filter out messages being delivered
       */
      auto atFrequency{[&v2vService]() -> bool {

        // Check when last FollowerStatus was received
        if(!v2vService->followerIp.empty() && v2vService->carConnectionLost(FOLLOWER_STATUS)){
          std::cout << "Follower lost!" << v2vService->presentCars[GROUP_ID] << std::endl;
          v2vService->stopFollow(v2vService->leaderIp);
        }

        // Check when last LeaderStatus was received
        if(!v2vService->leaderIp.empty() && v2vService->carConnectionLost(LEADER_STATUS)){
          std::cout << "Leader lost!" << v2vService->presentCars[GROUP_ID] << std::endl;
          v2vService->stopFollow(v2vService->followerIp);
        }

        // Constantly send messages
        v2vService->announcePresence();
        v2vService->followRequest(v2vService->presentCars[GROUP_ID]);
        v2vService->followerStatus();
        v2vService->leaderStatus();
        return true;
      }};
      // Send at higher frequency than 125ms to hopefully compensate for the latency
      internal->timeTrigger(FREQ, atFrequency);
    }
}
  


/**
 * Implementation of the V2VService class as declared in V2VService.hpp
 */
V2VService::V2VService() {
    /*
     * The broadcast field contains a reference to the broadcast channel which is an OD4Session. This is where
     * AnnouncePresence messages will be received.
     */
    broadcast =
        std::make_shared<cluon::OD4Session>(BROADCAST_CHANNEL,
          [this](cluon::data::Envelope &&envelope) noexcept {
              std::cout << "[OD4] ";
              switch (envelope.dataType()) {
                  case ANNOUNCE_PRESENCE: {
                      AnnouncePresence announcePresence = cluon::extractMessage<AnnouncePresence>(std::move(envelope));
                      std::cout << "received 'AnnouncePresence' from '"
                                << announcePresence.vehicleIp() << "', GroupID '"
                                << announcePresence.groupId() << "'!" << std::endl;
                      
                      presentCars[announcePresence.groupId()] = announcePresence.vehicleIp();
                      
                      // Transfer message to internal channel for data visualization
                      internal->send(announcePresence);
                      break;
                  }
                  default: std::cout << "Wrong channel dummy!" << std::endl;
              }
          });
  

    /*
     * Each car declares an incoming UDPReceiver for messages directed at them specifically. This is where messages
     * such as FollowRequest, FollowResponse, StopFollow, etc. are received.
     */
    incoming =
        std::make_shared<cluon::UDPReceiver>("0.0.0.0", DEFAULT_PORT,
           [this](std::string &&data, std::string &&sender, std::chrono::system_clock::time_point &&ts) noexcept {
               
               std::cout << "[UDP] ";
               std::pair<int16_t, std::string> msg = extract(data);

               switch (msg.first) {
                   case FOLLOW_REQUEST: {
                       FollowRequest followRequest = decode<FollowRequest>(msg.second);
                       std::cout << "received '" << followRequest.LongName()
                                 << "' from '" << sender << "'!" << std::endl;

                       // After receiving a FollowRequest, check first if there is currently no car already following.
                       if (followerIp.empty()) {
                           unsigned long len = sender.find(':');    // If no, add the requester to known follower slot
                           followerIp = sender.substr(0, len);      // and establish a sending channel.

                           toFollower = std::make_shared<cluon::UDPSender>(followerIp, DEFAULT_PORT);
                           followResponse();
                          
                           // Transfer message to internal channel for data visualization
                           internal->send(followRequest);
                       }
                       break;
                   }
                   case FOLLOW_RESPONSE: {
                       FollowResponse followResponse = decode<FollowResponse>(msg.second);
                       std::cout << "received '" << followResponse.LongName()
                                 << "' from '" << sender << "'!" << std::endl;

                       // Transfer message to internal channel for data visualization
                       internal->send(followResponse);
                       break;
                   }
                   case STOP_FOLLOW: {
                       StopFollow stopFollow = decode<StopFollow>(msg.second);
                       std::cout << "received '" << stopFollow.LongName()
                                 << "' from '" << sender << "'!" << std::endl;
                       // remove leader ip from map
                       presentCars[GROUP_ID] = "";
                       
                       // Clear either follower or leader slot, depending on current role.
                       unsigned long len = sender.find(':');
                       if (sender.substr(0, len) == followerIp) {
                           followerIp = "";
                           toFollower.reset();
                       }
                       else if (sender.substr(0, len) == leaderIp) {
                           leaderIp = "";
                           toLeader.reset();
                       }
                       
                       // Transfer message to internal channel for data visualization
                       internal->send(stopFollow);
                       break;
                   }
                   case FOLLOWER_STATUS: {
                       unsigned long len = sender.find(':');
                       FollowerStatus followerStatus = decode<FollowerStatus>(msg.second);
                       std::cout << "received '" << followerStatus.LongName()
                                 << "' from '" << sender << "'! " << std::endl;

                       // Reset time when last follower status update received
                       followerFreq = getTime();
                  
                       // Transfer message to internal channel for data visualization
                       internal->send(followerStatus);
                       break;
                   }
                   case LEADER_STATUS: {
                       unsigned long len = sender.find(':');
                       LeaderStatus leaderStatus = decode<LeaderStatus>(msg.second);
                       std::cout << "received '" << leaderStatus.LongName()
                                 << "' from '" << sender << "'!" 
				                 << "' Speed '" << leaderStatus.speed() << "'!"
				                 << "' Angle '" << leaderStatus.steeringAngle() << "'!"
                                 << "' Distance '" << leaderStatus.distanceTraveled() << "'!"
                                 << std::endl;
                       
                       // Reset time when last leader status update received
                       leaderFreq = getTime();

                       // Transfer message to internal channel for data visualization
                       internal->send(leaderStatus);
                       break;
                   }
                   default: std::cout << "¯\\_(ツ)_/¯" << std::endl;
               }
           });
}

/**
 * This function sends an AnnouncePresence (id = 1001) message on the broadcast channel. It will contain information
 * about the sending vehicle, including: IP, port and the group identifier.
 */
void V2VService::announcePresence() {
    if (!followerIp.empty() || !leaderIp.empty()) return;
    AnnouncePresence announcePresence;
    announcePresence.vehicleIp(DASH_IP);
    announcePresence.groupId(DASH_ID);
    broadcast->send(announcePresence);
    
    // Send message to internal channel for visualization
    internal->send(announcePresence);
}

/**
 * This function sends a FollowRequest (id = 1002) message to the IP address specified by the parameter vehicleIp. And
 * sets the current leaderIp field of the sending vehicle to that of the target of the request.
 *
 * @param vehicleIp - IP of the target for the FollowRequest
 */
void V2VService::followRequest(std::string vehicleIp) {
    if (!leaderIp.empty()) return;

    // Reset time when last follower status update received
    followerFreq = getTime();
    
    leaderIp = vehicleIp;
    toLeader = std::make_shared<cluon::UDPSender>(leaderIp, DEFAULT_PORT);
    FollowRequest followRequest;
    followRequest.status(1);
    toLeader->send(encode(followRequest));

    // Send message to internal channel for visualization
    internal->send(followRequest);
}

/**
 * This function send a FollowResponse (id = 1003) message and is sent in response to a FollowRequest (id = 1002).
 * This message will contain the NTP server IP for time synchronization between the target and the sender.
 */
void V2VService::followResponse() {
    if (followerIp.empty()) return;
     
    // Reset time when last follower status update received
    leaderFreq = getTime();

    FollowResponse followResponse;
    followResponse.status(1);
    toFollower->send(encode(followResponse));

    // Send message to internal channel for visualization
    internal->send(followResponse);
}

/**
 * This function sends a StopFollow (id = 1004) request on the ip address of the parameter vehicleIp. If the IP address
 * is neither that of the follower nor the leader, this function ends without sending the request message.
 *
 * @param vehicleIp - IP of the target for the request
 */
void V2VService::stopFollow(std::string vehicleIp) {

    // remove leader ip
    presentCars[GROUP_ID] = "";
    StopFollow stopFollow;

    if (vehicleIp == leaderIp) {
    	stopFollow.status(1);
      toLeader->send(encode(stopFollow));
      leaderIp = "";
      toLeader.reset();
    }
    if (vehicleIp == followerIp) {
      stopFollow.status(1);
      toFollower->send(encode(stopFollow));
      followerIp = "";
      toFollower.reset();
    }
    // Send message to internal channel for visualization
    internal->send(stopFollow);
}

/**
 * This function sends a FollowerStatus (id = 3001) message on the leader channel.
 */
void V2VService::followerStatus() {
  if (!leaderIp.empty()) {
    FollowerStatus followerStatus;
    followerStatus.status(1);
    toLeader->send(encode(followerStatus));

    // Send message to internal channel for visualization
    internal->send(followerStatus);
  }
}

/**
 * This function sends a LeaderStatus (id = 2001) message on the follower channel.
 *
 * @param speed - current velocity
 * @param steeringAngle - current steering angle
 * @param distanceTraveled - distance traveled since last reading
 */
void V2VService::leaderStatus() {    
  if (!followerIp.empty()) {
    LeaderStatus leaderStatus;
    leaderStatus.timestamp(getTime());
    leaderStatus.speed(PEDAL_SPEED);
    leaderStatus.steeringAngle(STEERING_ANGLE);
    leaderStatus.distanceTraveled(0);
    toFollower->send(encode(leaderStatus));

    // Send message to internal channel for visualization
    internal->send(leaderStatus);
  }
}

/**
 * This function addresses emergency scenarios where connection to the other car has been lost.
 *
 * @param timestamp - time of the latest
 * @param requestId - a constant int used to check which status update was received
 */
bool V2VService::carConnectionLost(int requestId) {
    double diff;
    if (FOLLOWER_STATUS == requestId) {
    	diff = getTime() - followerFreq;
      std::cout << "Time between FollowStatus: " << diff << "ms" << std::endl;
    }
    else if (LEADER_STATUS == requestId) {
      diff = getTime() - leaderFreq;
      std::cout << "Time between LeaderStatus: " << diff << "ms" << std::endl;
    }
    // Ignore the first message
    if(diff >= 1.524e+12){
      return false;
    }
    else if (diff >= TIME_DIFF){
	    return true;
    }

    return false;
}

std::string V2VService::getLeader() {
    return this->leaderIp;
}

std::string V2VService::getFollower() {
    return this->followerIp;
}
/**
 * Gets the current time.
 *
 * @return current time in milliseconds
 */
uint64_t V2VService::getTime() {
    using namespace std::chrono;
    
    milliseconds ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    );
    return (uint64_t) ms.count();
}

/**
 * The extraction function is used to extract the message ID and message data into a pair.
 *
 * @param data - message data to extract header and data from
 * @return pair consisting of the message ID (extracted from the header) and the message data
 */
std::pair<int16_t, std::string> V2VService::extract(std::string data) {
    if (data.length() < 10) return std::pair<int16_t, std::string>(-1, "");
    int id, len;
    std::stringstream ssId(data.substr(0, 4));
    std::stringstream ssLen(data.substr(4, 10));
    ssId >> std::hex >> id;
    ssLen >> std::hex >> len;
    return std::pair<int16_t, std::string> (
            data.length() -10 == len ? id : -1,
            data.substr(10, data.length() -10)
    );
};

/**
 * Generic encode function used to encode a message before it is sent.
 *
 * @tparam T - generic message type
 * @param msg - message to encode
 * @return encoded message
 */
template <class T>
std::string V2VService::encode(T msg) {
    cluon::ToProtoVisitor v;
    msg.accept(v);
    std::stringstream buff;
    buff << std::hex << std::setfill('0')
         << std::setw(4) << msg.ID()
         << std::setw(6) << v.encodedData().length()
         << v.encodedData();
    return buff.str();
}

/**
 * Generic decode function used to decode an incoming message.
 *
 * @tparam T - generic message type
 * @param data - encoded message data
 * @return decoded message
 */
template <class T>
T V2VService::decode(std::string data) {
    std::stringstream buff(data);
    cluon::FromProtoVisitor v;
    v.decodeFrom(buff);
    T tmp = T();
    tmp.accept(v);
    return tmp;
}
