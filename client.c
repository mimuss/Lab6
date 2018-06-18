#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>

struct Server {
  char ip[255];
  int port;
};

bool is_file_exist(const char *fileName)
{
    FILE *file;
    if (file = fopen(fileName, "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  uint64_t result = 0;
  a = a % mod;
  while (b > 0) {
    if (b % 2 == 1)
      result = (result + a) % mod;
    a = (a * 2) % mod;
    b /= 2;
  }

  return result % mod;
}

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0)
    return false;

  *val = i;
  return true;
}

int main(int argc, char **argv) {
  uint64_t k = -1;
  uint64_t mod = -1;
  char servers[255] = {'\0'}; // TODO: explain why 255

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        ConvertStringToUI64(optarg, &k);
        if (k <= 0)
          {
              printf("Invalid arguments (k)!\n");
              exit(EXIT_FAILURE);
          }
        break;
      case 1:
        ConvertStringToUI64(optarg, &mod);
        if (mod <= 0)
          {
              printf("Invalid arguments (mod)!\n");
              exit(EXIT_FAILURE);
          }
        break;
      case 2:
        if (is_file_exist(optarg))
        {
            memcpy(servers, optarg, strlen(optarg));
        }
        else
        {
            printf("Invalid arguments (servers)!\n");
            exit(EXIT_FAILURE);
        } 
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (k == -1 || mod == -1 || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  // TODO: for one server here, rewrite with servers from file
  unsigned int servers_num = 0;
  FILE* fp;
  fp = fopen(servers, "r");
  while (!feof(fp))
  {
    char test1[255];
    char test2[255];
    fscanf(fp, "%s : %s\n", test1, test2);
    servers_num++;
  }

  struct Server *to = malloc(sizeof(struct Server) * servers_num);
  fseek(fp, 0L, SEEK_SET);

  int index = 0;
  while (!feof(fp))
  {
    fscanf(fp, "%s : %d\n", to[index].ip, &to[index].port);
    printf("ip: %s, port: %d\n", to[index].ip, to[index].port);
    index++;
  }
  fclose(fp);

  // TODO: work continiously, rewrite to make parallel
  uint64_t result = 1;
  int* sck = malloc(sizeof(int) * servers_num);
  int interval = k / servers_num;
  int previous_end = interval;

  for (int i = 0; i < servers_num; i++) {
    struct hostent *hostname = gethostbyname(to[i].ip);
    if (hostname == NULL) {
      fprintf(stderr, "gethostbyname failed with %s\n", to[i].ip);
      exit(1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(to[i].port);
    server.sin_addr.s_addr = *((unsigned long *)hostname->h_addr);

    int sck = socket(AF_INET, SOCK_STREAM, 0);
    if (sck < 0) {
      fprintf(stderr, "Socket creation failed!\n");
      exit(1);
    }

    if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
      fprintf(stderr, "Connection failed\n");
      exit(1);
    }

    // TODO: for one server
    // parallel between servers

    uint64_t begin = 1;
    uint64_t end = interval;

    if (i != 0) {
      begin = previous_end + 1;
      end = i * interval;
      previous_end = i * interval;
    }

    char flag = 'f';

    char task[sizeof(uint64_t) * 3 + sizeof(char)];

    memcpy(task,                                       &flag,  sizeof(char));
    memcpy(task + sizeof(char),                        &begin, sizeof(uint64_t));
    memcpy(task + sizeof(uint64_t) + sizeof(char),     &end,   sizeof(uint64_t));
    memcpy(task + 2 * sizeof(uint64_t) + sizeof(char), &mod,   sizeof(uint64_t));

    if (send(sck, task, sizeof(task), 0) < 0) {
      fprintf(stderr, "Send failed\n");
      exit(1);
    }

    char response[sizeof(uint64_t)];
    if (recv(sck, response, sizeof(response), 0) < 0) {
      fprintf(stderr, "Recieve failed\n");
      exit(1);
    }

    // TODO: from one server
    // unite results
    uint64_t answer = 0;
    memcpy(&answer, response, sizeof(uint64_t));
    printf("answer: %llu\n", answer);

    close(sck);
  }
  free(to);

  return 0;
}
