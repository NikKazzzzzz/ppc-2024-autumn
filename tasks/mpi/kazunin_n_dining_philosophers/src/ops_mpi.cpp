#include "mpi/kazunin_n_dining_philosophers/include/ops_mpi.hpp"

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>
#include <cmath>

bool kazunin_n_dining_philosophers_mpi::DiningPhilosophersParallelMPI::validation() {
  internal_order_test();

  return *reinterpret_cast<double*>(taskData->inputs[0]) < 1 &&
         *reinterpret_cast<double*>(taskData->inputs[1]) < *reinterpret_cast<double*>(taskData->inputs[0]);
}

bool kazunin_n_dining_philosophers_mpi::DiningPhilosophersParallelMPI::pre_processing() {
  internal_order_test();

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  std::srand(std::time(nullptr) + rank);

  SIMULATION_TIME = *reinterpret_cast<double*>(taskData->inputs[0]);
  SLEEP_TIME = *reinterpret_cast<double*>(taskData->inputs[1]);

  leftNeighbor = (rank + size - 1) % size;
  rightNeighbor = (rank + 1) % size;

  hasLeftFork = false;
  hasRightFork = false;

  leftForkAvailable = rank > leftNeighbor;
  rightForkAvailable = rank > rightNeighbor;

  state = THINKING;
  thinkTime = (rand() % 3 + 1) * SLEEP_TIME;
  eatTime = (rand() % 3 + 1) * SLEEP_TIME;
  timeCounter = 0;

  return true;
}

bool kazunin_n_dining_philosophers_mpi::DiningPhilosophersParallelMPI::run() {
  internal_order_test();

  double startTime = MPI_Wtime();

  while (MPI_Wtime() - startTime < SIMULATION_TIME) {
    int flag;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

    if (flag != 0) {
      int message;
      MPI_Recv(&message, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

      if (status.MPI_TAG == REQUEST_FORK) {
        if (status.MPI_SOURCE == leftNeighbor && leftForkAvailable) {
          leftForkAvailable = false;
          message = 0;
          MPI_Send(&message, 1, MPI_INT, status.MPI_SOURCE, FORK_AVAILABLE, MPI_COMM_WORLD);
        } else if (status.MPI_SOURCE == rightNeighbor && rightForkAvailable) {
          rightForkAvailable = false;
          message = 0;
          MPI_Send(&message, 1, MPI_INT, status.MPI_SOURCE, FORK_AVAILABLE, MPI_COMM_WORLD);
        }
      } else if (status.MPI_TAG == FORK_AVAILABLE) {
        if (status.MPI_SOURCE == leftNeighbor) {
          hasLeftFork = true;
        } else if (status.MPI_SOURCE == rightNeighbor) {
          hasRightFork = true;
        }
      }
    }

    if (state == THINKING) {
      if (timeCounter < thinkTime) {
        usleep(SLEEP_TIME * 1000000);
        timeCounter += SLEEP_TIME;
      } else {
        state = HUNGRY;
        timeCounter = 0;
        thinkTime = (rand() % 3 + 1) * SLEEP_TIME;
      }
    } else if (state == HUNGRY) {
      if (!hasLeftFork) {
        int message = 0;
        MPI_Send(&message, 1, MPI_INT, leftNeighbor, REQUEST_FORK, MPI_COMM_WORLD);
      }
      if (!hasRightFork) {
        int message = 0;
        MPI_Send(&message, 1, MPI_INT, rightNeighbor, REQUEST_FORK, MPI_COMM_WORLD);
      }
      if (hasLeftFork && hasRightFork) {
        state = EATING;
        eatTime = (rand() % 3 + 1) * SLEEP_TIME;
        timeCounter = 0;
      }
    } else if (state == EATING) {
      if (timeCounter < eatTime) {
        usleep(SLEEP_TIME * 1000000);
        timeCounter += SLEEP_TIME;
      } else {
        state = THINKING;
        timeCounter = 0;

        hasLeftFork = false;
        hasRightFork = false;
        leftForkAvailable = true;
        rightForkAvailable = true;

        int message = 0;
        MPI_Send(&message, 1, MPI_INT, leftNeighbor, FORK_AVAILABLE, MPI_COMM_WORLD);
        MPI_Send(&message, 1, MPI_INT, rightNeighbor, FORK_AVAILABLE, MPI_COMM_WORLD);

        thinkTime = (rand() % 3 + 1) * SLEEP_TIME;
      }
    }
  }

  return true;
}

bool kazunin_n_dining_philosophers_mpi::DiningPhilosophersParallelMPI::post_processing() {
  internal_order_test();

  return true;
}