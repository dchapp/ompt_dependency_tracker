#ifndef DEPENDENCY_DAG_H
#define DEPENDENCY_DAG_H

#include <boost/graph/directed_graph.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adjacency_list.hpp>


//#include <VertexWriters.hpp> 
/* A vertex writer that only displays the task or parallel region ID on an 
 * ellipse-shaped vertex 
 */
template <class vertex_id_map>
class vertex_writer
{
  public:
    vertex_writer(vertex_id_map v_id_m) : v_id_map(v_id_m) {}
    template <class vertex>
    void operator() (std::ostream &out, const vertex& v) const {
      out << "[label=\""
          << v_id_map[v]
          << "\"]";
    }
  private: 
    vertex_id_map v_id_map;
};

/* Helper function for above-defined vertex writer */
template <class vertex_id_map>
inline vertex_writer<vertex_id_map>
make_vertex_writer(vertex_id_map v_id_m)
{
  return vertex_writer<vertex_id_map>(v_id_m);
}

///* https://stackoverflow.com/questions/11369115/how-to-print-a-graph-in-graphviz-with-multiple-properties-displayed
// * https://stackoverflow.com/questions/13106328/graphviz-nodes-of-different-colors
// */
//template <class vertex_id_map,
//          class vertex_type_map,
//          class color_map,
//          class shape_map,
//          class status_map,
//          class codeptr_ra_map,
//          class dependences_map
//         >
//class vertex_writer
//{
//  public:
//    vertex_writer(vertex_id_map vid_m, 
//                  vertex_type_map vtype_m,
//                  color_map c_m,
//                  shape_map s_m,
//                  status_map stat_m,
//                  codeptr_ra_map cra_m,
//                  dependences_map dep_m
//                 ) : vid_m(vid_m), 
//                     vtype_m(vtype_m),
//                     c_m(c_m),
//                     s_m(s_m),
//                     stat_m(stat_m),
//                     cra_m(cra_m),
//                     dep_m(dep_m)
//                 {}
//    template <class vertex>
//    void operator() (std::ostream &out,
//                     const vertex& v) const 
//    {
//      out << "[label=" 
//          << "\""
//          << vtype_m[v]
//          << "\n"
//          << vid_m[v] 
//          << "\n"
//          << "Status: "
//          << stat_m[v]
//          << "\n"
//          << "codeptr_ra: "
//          << cra_m[v]
//          << "\n";
//      if (dep_m[v].size() > 0) {
//        out << "Deps: ";
//        for (auto dep : dep_m[v]) {
//          out << dep
//              << ", ";
//        }
//      }
//      out << "\""
//          << ", shape="
//          << s_m[v]
//          << ", style=filled, fillcolor=" 
//          << c_m[v] 
//          << "]";
//    }
//  private:
//    vertex_id_map vid_m;
//    vertex_type_map vtype_m;
//    color_map c_m; 
//    shape_map s_m;
//    status_map stat_m;
//    codeptr_ra_map cra_m; 
//    dependences_map dep_m; 
//};
//
//
//// Helper function to construct custom vertex_writer 
//template <class vertex_id_map,
//          class vertex_type_map,
//          class color_map,
//          class shape_map,
//          class status_map,
//          class codeptr_ra_map,
//          class dependences_map
//         >
//inline vertex_writer<vertex_id_map, 
//                     vertex_type_map,
//                     color_map, 
//                     shape_map,
//                     status_map,
//                     codeptr_ra_map,
//                     dependences_map
//                    >
//make_vertex_writer(vertex_id_map vid_m, 
//                   vertex_type_map vtype_m,
//                   color_map c_m, 
//                   shape_map s_m,
//                   status_map stat_m,
//                   codeptr_ra_map cra_m,
//                   dependences_map dep_m
//                  )
//{
//  return vertex_writer<vertex_id_map, 
//                       vertex_type_map,
//                       color_map, 
//                       shape_map,
//                       status_map,
//                       codeptr_ra_map,
//                       dependences_map
//                      >(vid_m, 
//                        vtype_m,
//                        c_m, 
//                        s_m,
//                        stat_m,
//                        cra_m,
//                        dep_m
//                       ); 
//}





enum class VertexType {ExplicitTask, ImplicitTask, ParallelRegion}; 

// A vertex represents a task.
// A task can either be implicit, such as when an "omp parallel" directive
// creates one implicit task per thread, or explicit, such as when a task is
// created by an "omp task" directive. 
typedef struct vertex_properties {
  uint64_t vertex_id;
  std::string vertex_type;
  std::string color;
  std::string shape; 
  std::string status;
  std::string codeptr_ra;
  std::vector<std::string> dependences; 
} vprops_t;

struct edge_properties {
};

struct dag_properties {
};

// Define a type for the Dependency DAG 
typedef boost::adjacency_list
  < 
    // Store edges as an std::vector
    boost::vecS, 
    // Store vertices as an std::vector
    boost::vecS, 
    // Edges are directional
    boost::directedS,
    // Each vertex has a multi-label as defined above
    vertex_properties,
    // Each edge has a multi-label as defined above
    edge_properties, 
    // The DAG itself has a global properties as defined above 
    dag_properties,
    // Store edge list as an std::vector
    boost::vecS
  > dag_t;

typedef typename boost::graph_traits< dag_t >::vertex_descriptor vertex_t;
typedef typename boost::graph_traits< dag_t >::edge_descriptor edge_t; 



const vprops_t construct_vprops(VertexType vtype,
                                uint64_t omp_entity_id,
                                const void * codeptr_ra)
{
  // Fill out basic vertex properties based on the default for each kind of 
  // newly created entity
  std::string vtype_label;
  std::string color;
  std::string shape;
  std::string status;
  switch (vtype) 
  {
    case VertexType::ParallelRegion: 
    {
      vtype_label = "Parallel Region";
      color = "deeppink";
      shape = "box";
      status = "Running";
      break;
    }
    case VertexType::ImplicitTask:
    {
      vtype_label = "Implicit Task";
      color = "gold";
      shape = "trapezium";
      status = "Running";
      break;
    }
    case VertexType::ExplicitTask:
    {
      vtype_label = "Explicit Task";
      color = "gray";
      shape = "ellipse";
      status = "Created";
      break;
    }
  }
  
  // Convert codeptr_ra to string
  std::ostringstream oss;
  oss << codeptr_ra;
  std::string codeptr_ra_str(oss.str());

  // Construct vertex properties struct
  const vprops_t vp = {omp_entity_id,
                       vtype_label,
                       color,
                       shape,
                       status,
                       codeptr_ra_str};
  return vp; 
}

#endif // DEPENDENCY_DAG_H
