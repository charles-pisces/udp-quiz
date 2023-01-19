# Build base image
sudo docker build -f Dockerfile -t debian-10-base .;

# Run udp server container in background
sudo docker run -d -it --net host \
                -v /Users/charles/projects/udp-quiz/udp_server:/projects \
                --name "udp_server_test" debian-10-base;

# Run udp client container in background
sudo docker run -d -it --net host \
                -v /Users/charles/projects/udp-quiz/udp_client:/projects \
                --name "udp_client_test" debian-10-base;
