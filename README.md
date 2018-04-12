# DIT 168 V2V protocol proposal

This repo contains a V2V communication protocol between autonomous RCs with a focus on platooning. The RCs' specs can be found [here](https://github.com/chalmers-revere/opendlv.scaledcars).

### Table of contents

1. ##### [Installation](https://github.com/DIT168-V2V-responsibles/v2v-protocol#1-installation)
2. ##### [Get started](https://github.com/DIT168-V2V-responsibles/v2v-protocol#2-get-started)
3. ##### [License](https://github.com/DIT168-V2V-responsibles/v2v-protocol#3-license)
4. ##### [Requests](https://github.com/DIT168-V2V-responsibles/v2v-protocol#4-protocol-requests)
5. ##### [CID ranges](https://github.com/DIT168-V2V-responsibles/v2v-protocol#5-cid-ranges)
6. ##### [Emergency Scenarios](https://github.com/DIT168-V2V-responsibles/v2v-protocol#5-emergency-scenarios)

### 1. Installation
To install libcluon please refer to the installation guide [Libcluon](https://github.com/chrberger/libcluon).

### 2. Get started

Clone the repo using:
```
git clone https://github.com/DIT168-V2V-responsibles/v2v-protocol.git
```

Make a build folder and navigate into it:
```
mkdir build
cd build
```

Run `cmake` to create the necessary build files:
```
cmake ..
```

Finally compile:
```
make
```
**Note:** The protocol needs to be adapted to the specific needs of your group. It is **not** a complete microservice and you should therefore turn it into one if that's what you want to use it for.

### 3. License
The protocol is licenced under GNU Lesser General Public License version 3.0. This is due to the incorporation of "libcluon" library as part of the project. Libcluon offers their software under LGPLv 3.0 licence and due to the copyleft nature, anyone who distribute its code or derivative works, are required to make the source available under the same terms. 
Libcluon library can be found [here](https://github.com/chrberger/libcluon).

### 4. Protocol Requests 
This section describes the protocol requests. Fields of requests and their types are denoted: Type field name.

#### 4.1 Common Requests

##### Announce Presence
This message is intended for the cars not leading other cars yet, to inform their presence on the network. The Announce Presence holds the IP of the car as a unique identification among all the cars that followers can connect to, if they choose to follow the car sending Announce Presence message. Moreover, the car also includes the id of the group in the message in order to inform the cars about the group number.

***Fields***
* string   vehicleIp  - IP of the car that sends the announce presence, used as a unique idetifier.
* string   groupId    - The project group number of the group that has the car.

##### Follow Request  
This message is sent to the car that is about to be followed by another car that wants to initiate following. This message requires a response i.e. Follow Response. 

##### Follow Response
This message is sent in response to a Follow Request. The message is used in combination with the Follow Request message to establish direct Car to Car communication. 

##### Stop Follow Request
This message is sent by a car to indicate that following must come to an end. Both the leading and the following vehicles are able to send this request. This message does not expect a response.

#### 4.2 Leader Specific Requests

##### Status Update
This message includes information about a leading vehicle and contains information relevant for a following car to be able to follow it. The LeaderStatus is sent in regular intervals of 125ms and does not expect a response.

***Fields***
* uint32_t timestamp       - The time stamp (the time that the message has been sent) of the leading vehicle.
* float  speed           - Current speed of the leading vehicle.
* float steeringAngle    - Current steering angle of the leading vehicle.
* uint8_t distanceTraveled - The distance travelled since the last status update (according the odometer).

#### 4.3 Follower Specific Requests

##### Status Update
This message lets the leading car know that this follower is still following. The FollowerStatus is sent in regular intervals of 125ms and does not expect a response.

### 5. CID ranges

For the purposes of the DIT168 course, the OD4 session [CIDs](https://chrberger.github.io/libcluon/classcluon_1_1OD4Session.html#ad9d26426cf2714e105c27a23ce4a0f7a) that the project groups are going to use are listed below.

| Group | CID     |
| ----- | :-----: |
|   1   | 120-129 |
|   2   | 130-139 |
|   3   | 140-149 |
|   4   | 150-159 |
|   5   | 160-169 |
|   6   | 170-179 |
|   7   | 180-189 |
|   8   | 190-199 |
|   9   | 200-209 |
|   10  | 210-219 |
|   11  | 220-229 |
|   12  | 230-239 |
|   13  | 240-249 |

The Announce Presence messages between the groups will be broadcast to an OD4 session with CID **250**.

### 6. Emergency Scenarios

As part of the protocol the V2V managers have established a number of emergency scenarios. The purpose of this section is to describe the expected behaviour exhibited by the miniature cars if or when they encounter abnormal or problematic protocol behaviour. 

#### 6.1 Connection to Leader Lost
The leader vehicle sends it's status update, Leader Status, to the follower every 125ms. The problematic behaviour has been defined as the follower failing to receive the aforementioned request 3 times i.e. no leader update in 375ms. In this scenario the expected behaviour is for the follower to return to it's pre-platooning state, meaning to stop moving.

#### 6.2 Connection to Follower Lost
The follower vehicle sends Follower Status to the leader every 125ms. If the leading vehicle has not received this request in 375ms or over, the leader can drop it's connection to the follower and stop sending Leader Status.

#### 6.3 Object Detected 
The project groups have agreed to use the front ultrasonic sensor for collision prevention. It was decided that if the sensor readings indicate and object 10cm in front of the car, the car will stop, to avoid damaging the nice 3D-printed car. This behaviour is expected from both the leading and the following vehicles.
