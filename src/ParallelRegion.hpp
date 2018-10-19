#ifndef PARALLEL_REGION_H
#define PARALLEL_REGION_H

class ParallelRegion
{
  private:
    boost::mutex mtx; 
    uint64_t id;
    uint64_t parent_id;
    uint32_t n_threads;
    const void* codeptr_ra;
    std::vector<Task*> children; 

  public:
    ParallelRegion(uint64_t region_id, 
                   uint64_t parent_id, 
                   uint32_t n_threads, 
                   const void* codeptr_ra):
      id(region_id),
      parent_id(parent_id),
      n_threads(n_threads),
      codeptr_ra(codeptr_ra)
      {}

    uint64_t get_id() {
      return id; 
    }   
    
    uint64_t get_parent_id() {
      return parent_id;
    }

    const void* get_codeptr_ra() {
      return codeptr_ra; 
    }   

    void add_child(Task* child) {
      boost::lock_guard<boost::mutex> lock(this->mtx);
      this->children.push_back(child);
    }

    void print(int verbosity = 0) {
      boost::lock_guard<boost::mutex> lock(this->mtx); 
      if (verbosity == 0) {
        std::cout << "Parallel Region: " << id << std::endl;
      } 
      else {
        std::cout << "Parallel Region: " << std::endl;
        std::cout << "\t- ID: " << id << std::endl;
        std::cout << "\t- Parent ID: " << parent_id << std::endl;
        std::cout << "\t- Number of threads: " << n_threads << std::endl; 
        if (verbosity > 1) {
          std::cout << "Child Tasks: " << std::endl; 
          for (auto e : this->children) {
            e->print(1); 
          }
        }
      }
    }
};


#endif // PARALLEL_REGION_H 
