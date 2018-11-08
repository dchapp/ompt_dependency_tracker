#include <iostream>
#include "ToolData.hpp"
#include "DependencyDAG.hpp"

void add_parallel_region_vertex(ParallelRegion * pr, tool_data_t* tool_data_ptr) {
  // Synchronize access to the dag and the id_to_vertex map
  boost::lock_guard<boost::shared_mutex> dag_lock(tool_data_ptr->dag_mtx);
  boost::lock_guard<boost::shared_mutex> map_lock(tool_data_ptr->id_to_vertex_mtx);
  // First create vertex properties 
  const auto vp = construct_vprops(VertexType::ParallelRegion,
                                   pr->get_id(),
                                   pr->get_codeptr_ra());
  // Add a new vertex to the dag
  const vertex_t v = boost::add_vertex(vp, tool_data_ptr->dag);
  // Update the id_to_vertex map
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  auto res = id_to_vertex->insert( {pr->get_id(), v} ); 
}

vertex_t add_task_vertex(Task * t, tool_data_t* tool_data_ptr) {
  // Synchronize access to the dag and the id_to_vertex map
  //boost::lock_guard<boost::shared_mutex> dag_lock(tool_data_ptr->dag_mtx);
  //boost::lock_guard<boost::shared_mutex> map_lock(tool_data_ptr->id_to_vertex_mtx);
  // First create vertex properties 
  VertexType vt;
  if (t->get_type() == TaskType::Explicit) {
    vt = VertexType::ExplicitTask;
  } else {
    vt = VertexType::ImplicitTask;
  }
  const auto vp = construct_vprops(vt, t->get_id(), t->get_codeptr_ra());
  // Add a new vertex to the dag
  const vertex_t v = boost::add_vertex(vp, tool_data_ptr->dag);
  // Update the id_to_vertex map
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  auto res = id_to_vertex->insert( {t->get_id(), v} ); 
  return v; 
}


void add_vertices(tool_data_t * tool_data) {
  // Add vertices for parallel regions 
  auto id_to_region = tool_data->id_to_parallel_region;
  for (auto e : id_to_region) {
     add_parallel_region_vertex(e.second, tool_data);   
  } 
  // Add vertices for tasks
  auto id_to_task = tool_data->id_to_task;
  for (auto e : id_to_task) {
     vertex_t v = add_task_vertex(e.second, tool_data);   
  } 
}

//void link_region_with_parent(ParallelRegion * pr) {
//  uint64_t region_id = pr->get_id();
//  uint64_t parent_id = pr->get_parent_id(); 
//  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
//  auto region_search = id_to_vertex->find(region_id);
//  auto parent_search = id_to_vertex->find(parent_id);
//  if (region_search != id_to_vertex->end() && parent_search != id_to_vertex->end()) {
//    vertex_t region_vertex = region_search->second;
//    vertex_t parent_vertex = parent_search->second;
//    boost::add_edge(parent_vertex, region_vertex, tool_data_ptr->dag); 
//  }
//}
//
//void link_task_with_parent(Task * t) {
//  // Exit early if this is the initial task
//  if (t->is_initial()) {
//    return;
//  } 
//  uint64_t task_id = t->get_id();
//  uint64_t parent_id = t->get_parent_id(); 
//  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
//  auto task_search = id_to_vertex->find(task_id);
//  auto parent_search = id_to_vertex->find(parent_id);
//  if (task_search != id_to_vertex->end() && parent_search != id_to_vertex->end()) {
//    vertex_t task_vertex = task_search->second;
//    vertex_t parent_vertex = parent_search->second;
//    boost::add_edge(parent_vertex, task_vertex, tool_data_ptr->dag); 
//  }
//}


void add_edges(tool_data_t * tool_data) { 
  boost::lock_guard<boost::shared_mutex> dag_lock(tool_data_ptr->dag_mtx);
  boost::lock_guard<boost::shared_mutex> map_lock(tool_data_ptr->id_to_vertex_mtx);
  auto id_to_task = tool_data->id_to_task;
  auto dependences = tool_data->dependences;
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  for (auto e : dependences) {
    vertex_t src_v = id_to_vertex->find(e.first)->second;  
    for ( auto dependent_task : e.second ) {
      vertex_t dst_v = id_to_vertex->find( dependent_task )->second;
      boost::add_edge(src_v, dst_v, tool_data_ptr->dag);
    }
  } 
} 


void build_dag(tool_data_t * tool_data) {
  add_vertices(tool_data);
  add_edges(tool_data); 
}

void write_dag(dag_t dag) {
  printf("Writing DAG\n"); 
  // Get dotfile name for task tree visualization from environment
  char * env_var;
  std::string dag_dotfile;
  env_var = getenv("TASK_DAG_DOTFILE");
  if (env_var == NULL) {
    printf("TASK_DAG_DOTFILE not specified\n");
    dag_dotfile = "./dependency_dag.dot"; 
  } else {
    dag_dotfile = env_var;
  }
  // Open the stream to the dotfile for the task ancestry dag
  std::ofstream out(dag_dotfile);
  // Construct custom vertex writer
  auto dag_vw = make_vertex_writer(
    boost::get(&vertex_properties::vertex_id, dag) //,
    //boost::get(&vertex_properties::vertex_type, dag),
    //boost::get(&vertex_properties::color, dag),
    //boost::get(&vertex_properties::shape, dag),
    //boost::get(&vertex_properties::status, dag),
    //boost::get(&vertex_properties::codeptr_ra, dag),
    //boost::get(&vertex_properties::dependences, dag)
  );
  // Write out the task ancestry dag 
  boost::write_graphviz(out, dag, dag_vw); 

}
