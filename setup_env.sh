# Build base image
sudo docker build -f Dockerfile -t debian-10-base .;

# Config work space here
workspace=/Users/charles/projects

# Remove the old container first
sudo docker rm -v -f udp_server;
sudo docker rm -v -f udp_client;

# Run udp server container in background
sudo docker run -d -it --net host \
                -v $workspace/udp-quiz/udp_server:/projects \
                --name "udp_server" debian-10-base;

# Run udp client container in background
sudo docker run -d -it --net host \
                -v $workspace/udp-quiz/udp_client:/projects \
                --name "udp_client" debian-10-base;
