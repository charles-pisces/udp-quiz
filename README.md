# udp-quiz
udp-quiz

1. Setup env
   1.1 Create working space
       mkdir -p /User/charles/projects
   1.2 Download source code
       cd /User/charles/projects
       git clone https://github.com/charles-pisces/udp-quiz.git
   1.3 Build debian 10.0 base images
       cd udp-quiz
       ./setup_env.sh
   1.4 Check the container is running successfully
       sudo docker ps -a
       //check if udp_server / udp_client container exist or not 
   1.5 Enter udp_server container and build/run udp_server
       ./run_docker_udp_server.sh
       ./build.sh
       ./run_server.sh
    1.6 Enter udp_client container and build/run udp_server
       ./run_docker_udp_client.sh
       ./build.sh
       ./run_client.sh


2. How to run
    * UDP Server
        ./udp_server <server_port>
        - server_port: The server port
        
        Ex:
        ./udp_server 12345


    * UDP Client
        ./udp_client <server_iP> <server_port> <message> <max_retry>
        - server_ip:   The server ip
        - server_port: The server port
        - message:     The message sent to server. 0 < Message length < 255
        - max_retry:   The max retry count that no echo data received from server. 
                        Range: 0 < Max-Retry <= 10. 
  
        Ex:
        ./udp_client 127.0.0.1 12345 packet_id 3

3. Notice
   - Run more than one udp_client container to hit the max retry condition

