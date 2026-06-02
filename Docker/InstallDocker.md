### 1. Update packages and install prerequisites

```
sudo apt update
sudo apt install -y \
    ca-certificates \
    curl \
    gnupg \
    lsb-release
```

###  2. Add Docker’s official GPG key

```
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
  sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
```

###  3. Set up the Docker repository

```
echo \
  "deb [arch=$(dpkg --print-architecture) \
  signed-by=/etc/apt/keyrings/docker.gpg] \
  https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
```

### 4. Update again and install Docker Engine
```
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

```

### 5. Verify instalation 

```
sudo docker run hello-world
```

### 6.  (Optional) Run Docker without sudo

```
sudo usermod -aG docker $USER
newgrp docker
```

## Usefull Commands

| Command                   | Description                        |
| ------------------------- | ---------------------------------- |
| `docker ps`               | List running containers            |
| `docker images`           | List downloaded images             |
| `docker build -t myimg .` | Build image from Dockerfile        |
| `docker run -it myimg`    | Run a container interactively      |
| `docker compose up`       | Run containers with Docker Compose |
