#installation file
sudo apt-get update
java -version

## you need to install default java first
sudo apt-get install default-jre
sudo apt-get install default-jdk
sudo apt-get install oracle-java8-installer
sudo apt-get install python-software-properties
### this is to install java 8
sudo add-apt-repository ppa:webupd8team/java
sudo apt-get update
sudo apt-get install oracle-java8-installer
# additional files that need to be installed for swift
sudo apt-get update
sudo apt-get install -y python python-pip git
sudo pip install apache-libcloud

#  this is to install  the yadoo cloud
git clone https://github.com/yadudoc/cloud-tutorials.git
cd cloud-tutorials/ec2






