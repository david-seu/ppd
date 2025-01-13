#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <functional>
#include <algorithm>
#include <queue>
#include <tuple>
#include <condition_variable>

struct Variable {
    int value;                
    bool owned;              
    std::set<int> subscribers; 
};

struct Message {
    int clock;          
    int senderId;      
    std::string content; 

    bool operator<(const Message& other) const {
        if (clock != other.clock)
            return clock > other.clock; 
        return senderId > other.senderId; 
    }
};

class DsmNode {
    int nodeId;               
    int port;                 
    int sockfd;              
    int lamportClock;        
    std::map<int, Variable> variables;     
    std::map<int, sockaddr_in> peers;        
    std::function<void(int,int)> onChange;  
    std::mutex m;                            
    bool running;                           
    std::thread serverThread;                


    void incrementClock() {
        lamportClock++;
    }

    void updateClock(int incoming) {
        lamportClock = std::max(lamportClock, incoming) + 1;
    }


    std::priority_queue<Message> messageQueue;

    std::mutex queueMutex;
    std::condition_variable cv;

    std::thread processingThread;

    bool processingRunning;

    void serverLoop() {
        listen(sockfd, 10);  
        while (running) {
            sockaddr_in client;
            socklen_t len = sizeof(client);
            int clientSock = accept(sockfd, (sockaddr*)&client, &len);
            if (clientSock < 0) continue;
            std::thread(&DsmNode::handleClient, this, clientSock).detach();
        }
    }

    void handleClient(int clientSock) {
        char buf[1024];
        while (true) {
            int bytes = recv(clientSock, buf, 1024, 0);
            if (bytes <= 0) break;
            buf[bytes] = 0;
            std::cout << "Node " << nodeId << " received: " << buf << "\n";

            std::istringstream iss(buf);
            std::string header, cmd;
            int msgClock, senderId;
            iss >> header >> msgClock >> senderId >> cmd;
            if (header != "LC") continue; /
           
            std::string args;
            std::getline(iss, args);
            if (!args.empty() && args[0] == ' ')
                args.erase(0, 1);

            enqueueMessage(msgClock, senderId, cmd, args);
        }
        close(clientSock);
    }

    void enqueueMessage(int msgClock, int senderId, const std::string& cmd, const std::string& args) {
        Message msg;
        msg.clock = msgClock;
        msg.senderId = senderId;
        msg.content = cmd + " " + args;

        {
            std::lock_guard<std::mutex> lockClock(m);
            updateClock(msgClock);  

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            messageQueue.push(msg);
        }
        cv.notify_one();
    }


    void processMessages() {
        while (processingRunning) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this]() { return !messageQueue.empty() || !processingRunning; });
            if (!processingRunning) break;

            Message msg = messageQueue.top();

            messageQueue.pop();
            lock.unlock(); 
            handleMessageContent(msg.clock, msg.senderId, msg.content);
        }
    }

    void handleMessageContent(int msgClock, int senderId, const std::string& content) {
        std::istringstream iss(content);
        std::string cmd;
        iss >> cmd;

        if (cmd == "SETREQ") {
            int varId, val;
            iss >> varId >> val;
            std::lock_guard<std::mutex> lock(m);
            if (!variables.count(varId)) return;
            if (variables[varId].owned) {
                variables[varId].value = val;
                if (onChange) onChange(varId, val);
                broadcastSet(varId, val);
            } else {
                forwardSetReq(varId, val);
            }
        }
        else if (cmd == "CMPXCHGREQ") {
            int varId, oldVal, newVal;
            iss >> varId >> oldVal >> newVal;
            std::lock_guard<std::mutex> lock(m);
            if (!variables.count(varId)) return;
            if (variables[varId].owned) {
                if (variables[varId].value == oldVal) {
                    variables[varId].value = newVal;
                    if (onChange) onChange(varId, newVal);
                    broadcastSet(varId, newVal);
                }
            } else {
                forwardCmpxchgReq(varId, oldVal, newVal);
            }
        }
        else if (cmd == "SET") {
            int varId, val;
            iss >> varId >> val;
            std::lock_guard<std::mutex> lock(m);
            if (!variables.count(varId)) return;
            variables[varId].value = val;
            if (onChange) onChange(varId, val);
        }
    }

    void sendMessage(int peerId, const std::string &msg) {
        if (!peers.count(peerId)) return;
        std::cout << "Node " << nodeId << " sending to " << peerId << ": " << msg << "\n";
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            std::cerr << "Error creating socket\n";
            return;
        }
        if (connect(s, (sockaddr*)&peers[peerId], sizeof(peers[peerId])) < 0) {
            std::cerr << "Error connecting to peer " << peerId << "\n";
            close(s);
            return;
        }
        send(s, msg.c_str(), msg.size(), 0);
        close(s);
    }

    void broadcastSet(int varId, int val) {
        incrementClock();  
        for (auto &sub : variables[varId].subscribers) {
            if (sub == nodeId) continue;
            std::ostringstream oss;
            oss << "LC " << lamportClock << " " << nodeId << " SET " << varId << " " << val;
            sendMessage(sub, oss.str());
        }
    }

    void forwardSetReq(int varId, int val) {
        int owner = *variables[varId].subscribers.begin();
        if (owner == nodeId) return;
        incrementClock(); 
        std::ostringstream oss;
        oss << "LC " << lamportClock << " " << nodeId << " SETREQ " << varId << " " << val;
        sendMessage(owner, oss.str());
    }

    void forwardCmpxchgReq(int varId, int oldVal, int newVal) {
        int owner = *variables[varId].subscribers.begin();
        if (owner == nodeId) return;
        incrementClock(); 
        std::ostringstream oss;
        oss << "LC " << lamportClock << " " << nodeId << " CMPXCHGREQ "
            << varId << " " << oldVal << " " << newVal;
        sendMessage(owner, oss.str());
    }

public:
    DsmNode(int id, int basePort, const std::map<int,std::set<int>> &subs,
            const std::function<void(int,int)> &cb)
        : nodeId(id), port(basePort + id), lamportClock(0), onChange(cb), running(true), processingRunning(true)
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Error creating socket\n";
            exit(1);
        }
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Error binding socket to port " << port << "\n";
            exit(1);
        }

        serverThread = std::thread(&DsmNode::serverLoop, this);

        for (auto &kv : subs) {
            int varId = kv.first;
            if (kv.second.count(id)) {
                Variable v;
                v.value = 0;
                v.owned = (*kv.second.begin() == id);
                v.subscribers = kv.second;
                variables[varId] = v;
            }
        }

        processingThread = std::thread(&DsmNode::processMessages, this);
    }

    void stop() {
        running = false;
        processingRunning = false;
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        cv.notify_all();
        if (serverThread.joinable()) serverThread.join();
        if (processingThread.joinable()) processingThread.join();
    }

    void addPeer(int peerId, const std::string &ip, int basePort) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(basePort + peerId);
        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
            std::cerr << "Invalid IP address: " << ip << "\n";
            return;
        }
        peers[peerId] = addr;
    }

    void writeVar(int varId, int val) {
        std::lock_guard<std::mutex> lock(m);
        incrementClock();
        if (!variables.count(varId)) return;
        if (variables[varId].owned) {
            variables[varId].value = val;
            if (onChange) onChange(varId, val);
            broadcastSet(varId, val);
        } else {
            forwardSetReq(varId, val);
        }
    }

    void compareExchange(int varId, int oldVal, int newVal) {
        std::lock_guard<std::mutex> lock(m);
        incrementClock(); 
        if (!variables.count(varId)) return;
        if (variables[varId].owned) {
            if (variables[varId].value == oldVal) {
                variables[varId].value = newVal;
                if (onChange) onChange(varId, newVal);
                broadcastSet(varId, newVal);
            }
        } else {
            forwardCmpxchgReq(varId, oldVal, newVal);
        }
    }

    int readVar(int varId) {
        std::lock_guard<std::mutex> lock(m);
        if (!variables.count(varId)) return -1;
        return variables[varId].value;
    }

    void printFinalValues() {
        std::lock_guard<std::mutex> lock(m);
        std::cout << "\nNode " << nodeId << " final values:\n";
        for (auto &pair : variables) {
            std::cout << "  Var " << pair.first 
                      << " = " << pair.second.value << "\n";
        }
        std::cout << std::endl;
    }

private:
    void processSet(int varId, int val) {
        if (!variables.count(varId)) return;
        variables[varId].value = val;
        if (onChange) onChange(varId, val);
    }

    void processSetReq(int varId, int val) {
        if (!variables.count(varId)) return;
        if (variables[varId].owned) {
            variables[varId].value = val;
            if (onChange) onChange(varId, val);
            broadcastSet(varId, val);
        } else {
            forwardSetReq(varId, val);
        }
    }

    void processCmpxchgReq(int varId, int oldVal, int newVal) {
        if (!variables.count(varId)) return;
        if (variables[varId].owned) {
            if (variables[varId].value == oldVal) {
                variables[varId].value = newVal;
                if (onChange) onChange(varId, newVal);
                broadcastSet(varId, newVal);
            }
        } else {
            forwardCmpxchgReq(varId, oldVal, newVal);
        }
    }
};
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./dsm <nodeId>\n";
        return 1;
    }
    int nodeId = std::stoi(argv[1]);
    std::map<int, std::set<int>> subs = {
        {1, {0}},
        {2, {0, 1}},
        {3, {1}},
        {4, {1, 2}},
        {5, {2}},
        {6, {2}}
    };

    auto cb = [&](int varId, int val) {
        std::cout << "Node " << nodeId << " var " << varId << " changed to " << val << "\n";
    };

    DsmNode node(nodeId, 5000, subs, cb);
    std::this_thread::sleep_for(std::chrono::seconds(3));

    if (nodeId == 0) {
        node.addPeer(1, "127.0.0.1", 5000);
        node.addPeer(2, "127.0.0.1", 5000);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        node.writeVar(1, 10);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        node.writeVar(2, 20);
    }
    else if (nodeId == 1) {
        node.addPeer(0, "127.0.0.1", 5000);
        node.addPeer(2, "127.0.0.1", 5000);
        node.writeVar(2, 30);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        node.writeVar(3, 40);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        node.compareExchange(2, 30, 35);
    }
    else {
        node.addPeer(0, "127.0.0.1", 5000);
        node.addPeer(1, "127.0.0.1", 5000);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        node.writeVar(4, 50);
        node.writeVar(5, 60);
        node.compareExchange(4, 50, 55);
    }

    std::cout << "Node " << nodeId << " - press Enter to see final values...\n";
    std::cin.get();
    node.printFinalValues();

    std::cout << "Node " << nodeId << " - press Enter to exit...\n";
    std::cin.get();

    node.stop();
    return 0;
}
