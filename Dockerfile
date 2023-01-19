from debian:10.0

env WORKSPACE=/projects

workdir ${WORKSPACE}
run apt-get update
run apt-get install -y build-essential 