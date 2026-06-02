#!/bin/bash
set -e

echo "Updating system packages..."
sudo apt update

echo "📦 Installing required dependencies..."
sudo apt install -y \
    ca-certificates \
    curl \
    gnupg \
    lsb-release \
    software-properties-common

echo "🔐 Setting up Docker GPG key..."
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
  sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

echo "📂 Adding Docker repository to APT sources..."
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
  https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

echo "🔄 Updating APT again..."
sudo apt update

echo "🐳 Installing Docker Engine and tools..."
sudo apt install -y docker-ce docker-ce-cli containerd.io \
  docker-buildx-plugin docker-compose-plugin

echo "Docker installation complete."

echo "🔍 Testing Docker installation..."
sudo docker run hello-world

echo "👥 Adding current user to 'docker' group..."
sudo usermod -aG docker $USER

echo "✅ Done. Please log out and log back in or run 'newgrp docker' to apply the group change."
