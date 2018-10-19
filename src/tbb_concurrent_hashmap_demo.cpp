#include "omp.h"
#include <vector>
#include <iostream> 
#include <unordered_map>
//#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_hash_map.h"

int main() {
  tbb::concurrent_hash_map<int, int> tbb_um;
  tbb::concurrent_hash_map<int, int>::accessor a; 

  std::unordered_map<int, int> std_um;

  std::vector<std::pair<int, int>> vec;
  vec.push_back({1,1});
  vec.push_back({1,9});
  vec.push_back({2,1});
  vec.push_back({2,2});
  vec.push_back({2,7});
  vec.push_back({3,0});
  vec.push_back({3,0});
  vec.push_back({3,0});
  vec.push_back({3,0});
  vec.push_back({3,10});

  const int vec_size = vec.size();

  #pragma omp parallel

  #pragma omp for
  for (int i = 0; i < vec_size; ++i) {
    int key = vec[i].first;
    int value = vec[i].second; 
    auto found = tbb_um.find(a, key); 
    if (found == true) {
      //it->second += vec[i].second;
      //std::cout << "Already present key = " << key << std::endl; 
      //int new_value = a->second + value;
      //tbb_um.erase(key);
      a->second += value; 
      a.release(); 
    }   
    else { 
      //tbb_um.insert(vec[i]); 
      //std::cout << "New key = " << key << std::endl; 
      tbb_um.insert( {key, value} );
    }
  }
  
  //#pragma omp for
  //for (int i = 0; i < vec_size; ++i) {
  //  auto it = std_um.find(vec[i].first); 
  //  if (it != std_um.end()) {
  //    it->second += vec[i].second;
  //  }   
  //  else { std_um.insert(vec[i]); }
  //}

  #pragma omp barrier 

  std::cout << "tbb::concurrent_unordered_map" << std::endl; 
  for (auto kv : tbb_um) {
    std::cout << "k: " << kv.first << "\t v: " << kv.second << std::endl;
  }

  //std::cout << "std::unordered_map" << std::endl; 
  //for (auto kv : std_um) {
  //  std::cout << "k: " << kv.first << "\t v: " << kv.second << std::endl;
  //}

  return 1;
}

