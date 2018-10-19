#ifndef TASK_H
#define TASK_H

#include <inttypes.h>
#include <stdio.h>
#include "boost/thread/mutex.hpp"
#include "boost/thread/locks.hpp"


#define CHECK_INSERTIONS


enum class TaskType {Explicit, Implicit};
enum class TaskState {Created, Running, Suspended, Canceled, Completed};
enum class TSP {task_create,
                task_complete,
                task_yield,
                task_wait,
                task_group_end,
                implicit_barrier,
                explicit_barrier,
                target_region_create,
                target_update,
                target_enter_data,
                target_exit_data,
                omp_target_memcpy,
                omp_target_memcpy_rect}; 

class Task
{
  private:
    boost::mutex mtx; 
    uint64_t id;
    uint64_t parent_id;
    TaskType type;
    TaskState state;
    bool initial; 
    const void * codeptr_ra; 
    // A list of threads in the order they encounter this task
    std::vector<uint64_t> encountering_threads; 
    // A list of tasks that were created by this task
    std::vector<Task *> ancestry_children;
    // A list of tasks that depend on this task
    std::vector<Task *> dependency_children;
    // A list of tasks that this task depends on 
    std::vector<Task *> dependency_parents;
    // A series of task scheduling points experienced by this task
    std::vector<int> tsps;

  public:
    Task(uint64_t task_id, 
         uint64_t parent_id, 
         uint64_t thread_id,
         TaskType task_type, 
         const void* codeptr_ra) : 
      id(task_id), 
      parent_id(parent_id), 
      type(task_type), 
      state(TaskState::Created), 
      initial(false),
      codeptr_ra(codeptr_ra),
      encountering_threads({thread_id})
      {}

    // Accessors
    std::vector<Task*> get_children() {
      return this->ancestry_children; 
    }
    
    TaskType get_type() {
      return this->type;
    }
    
    TaskState get_state() {
      return this->state;
    }

    uint64_t get_id() {
      return this->id;
    } 
    
    uint64_t get_parent_id() {
      return this->parent_id;
    } 

    const void* get_codeptr_ra() {
      return this->codeptr_ra; 
    }

    bool is_initial() {
      return initial; 
    }

    // Mutators 
    void add_tsp(ompt_task_status_t tsp) {
      boost::lock_guard<boost::mutex> lock(this->mtx);
      this->tsps.push_back(tsp);   
    }

    void add_child(Task * child) {
      boost::lock_guard<boost::mutex> lock(this->mtx);
      this->ancestry_children.push_back(child); 
    }

    void change_state(TaskState new_state) {
      boost::lock_guard<boost::mutex> lock(this->mtx);
      this->state = new_state;         
    }                                  
                                       
    void set_as_initial_task() {       
      boost::lock_guard<boost::mutex> lock(this->mtx);
      this->initial = true;         
    }                                  
                                       
    void print(int verbosity = 0) {
      boost::lock_guard<boost::mutex> lock(this->mtx);
      if (verbosity == 0) {
        printf("Task: %lu\n", this->id);
      }
      else {
        // Print task ID and parent ID
        printf("Task:\n");
        printf("\t- ID: %lu\n", this->id);
        printf("\t- Parent ID: %lu\n", this->parent_id);
        // Print task type 
        if (this->type == TaskType::Explicit) {
          printf("\t- Type: Explicit\n");
        } else if (this->type == TaskType::Implicit) {
          printf("\t- Type: Implicit\n");
        } else {
          printf("\t- Type: Unrecognized\n");
        }
        // Print task state
        if (this->state == TaskState::Created) {
          printf("\t- State: Created\n");
        } else if (this->state == TaskState::Running) {
          printf("\t- State: Running\n");
        } else if (this->state == TaskState::Suspended) {
          printf("\t- State: Suspended\n");
        } else if (this->state == TaskState::Canceled) {
          printf("\t- State: Canceled\n");
        } else if (this->state == TaskState::Completed) {
          printf("\t- State: Completed\n");
        } else {
          printf("\t- State: Unrecognized\n");
        }
        // Print whether this was the initial task or not
        if (this->is_initial() == true) {
          printf("\t- Initial Task: True\n");
        } else {
          printf("\t- Initial Task: False\n");
        }
     
        if (verbosity > 1) {
          // Print the series of encountering thread IDs
          printf("\t- Encountering Thread IDs: ");
          for (auto e : encountering_threads) {
            printf("%lu, ", e);
          }
          printf("\n"); 
          // Print the series of task scheduling points this task experience
          printf("\t- Task Scheduling Points: ");
          for (auto e : tsps) {
            printf("%d, ", e);
          }
          printf("\n"); 
          // Print tasks that have this task as a dependency
          printf("\t- Tasks that depend on this task:\n");
          for (auto e : dependency_children) {
            e->print();
          }
          // Print this task's dependencies
          printf("\t- Tasks that this task depends on:\n");
          for (auto e : dependency_parents) {
            e->print();
          }
        }
      }
    } 
  
}; 


#endif // TASK_H 
