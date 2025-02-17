#include "prt_tree.h"

PRT_Tree::PRT_Tree(int vertices_per_leaf, int sons_num) : Tree<Node_V>(sons_num)
{
    this->vertices_threshold = vertices_per_leaf;
    this->mesh = Mesh();
    this->root = Node_V();
}

void PRT_Tree::build_tree()
{
    for(itype i=1;i<=this->mesh.get_vertices_num(); i++)
    {
        this->add_vertex(this->root,this->mesh.get_domain(),0,i);
    }
    for(itype i=1;i<=this->mesh.get_triangles_num();i++)
    {
        this->add_triangle(this->root,this->mesh.get_domain(),0,i);
    }
}

void PRT_Tree::add_vertex(Node_V& n, Box& domain, int level, itype v)
{
    if (n.is_leaf())
    {
        n.add_vertex(v);
        if(is_full(n))
            this->split(n,domain,level);
    }
    else
    {
        for (int i = 0; i < this->subdivision.son_number(); i++)
        {
            Box son_dom = this->subdivision.compute_domain(domain,level,i);
            int son_level = level +1;
            if (son_dom.contains(this->mesh.get_vertex(v),this->mesh.get_domain().get_max()))
            {
                this->add_vertex(*n.get_son(i),son_dom,son_level,v);
                break;
            }
        }
    }
}

void PRT_Tree::add_triangle(Node_V& n, Box& domain, int level, itype t)
{
    if (!Geometry_Wrapper::triangle_in_box_build(t,domain,this->mesh)) return;

    if(n.is_leaf())
        n.add_triangle(t);
    else
    {
        for(int i=0;i<this->subdivision.son_number();i++)
        {
            Box son_dom = this->subdivision.compute_domain(domain,level,i);
            int son_level = level +1;
            this->add_triangle(*n.get_son(i),son_dom,son_level,t);
        }
    }
}

void PRT_Tree::split(Node_V& n, Box& domain, int level)
{
    n.init_sons(this->subdivision.son_number());
    //initilize the son nodes
    for(int i=0;i<this->subdivision.son_number();i++)
    {
        Node_V* s = new Node_V();
        n.set_son(s,i);
        s->set_parent(&n);
    }

    //re-insert the vertices
    for(RunIterator runIt = n.v_array_begin_iterator(), runEnd = n.v_array_end_iterator(); runIt != runEnd; ++runIt)
        this->add_vertex(n,domain,level,*runIt);

    //re-insert the triangles
    for(RunIterator runIt = n.t_array_begin_iterator(), runEnd = n.t_array_end_iterator(); runIt != runEnd; ++runIt)
        this->add_triangle(n,domain,level,*runIt);

    //delete the arrays of the node
    n.clear_v_array();
    n.clear_t_array();
}

void PRT_Tree::build_tree(Soup &soup)
{
    this->mesh.set_domain(soup.get_domain());
    int counter = 1;

    for(int i=1; i<=soup.get_triangles_num(); i++)
    {
        Explicit_Triangle &etri = soup.get_triangle(i);
        ivect indexed_tri;
        for(int v=0; v<etri.vertices_num(); v++)
        {
            if (mesh.get_domain().contains(etri.get_vertex(v),mesh.get_domain().get_max()))
            {
                bool first_time = false;
                this->add_vertex_from_soup(this->root,mesh.get_domain(),0,etri.get_vertex(v),counter,indexed_tri,first_time);
                if(first_time)
                    counter++;
            }
        }
        Triangle t = Triangle(indexed_tri);
        mesh.add_triangle(t);
    }

    for(int i=1;i<=this->mesh.get_triangles_num();i++)
    {
        this->add_triangle(this->root,this->mesh.get_domain(),0,i);
    }
}

void PRT_Tree::add_vertex_from_soup(Node_V &n, Box &domain, int level, Vertex &v, itype vertex_index, ivect &indexed_tri, bool &first_time)
{
    if (n.is_leaf())
    {
        int ind = this->is_already_inserted(n,v);
        if(ind == -1)
        {
            ind = vertex_index;
            mesh.add_vertex(v);
            first_time = true;
            n.add_vertex(ind);
            /// in case of split we call the "usual" addVertex because we have all we need into the mesh variable
            if (is_full(n))
            {
                this->split(n,domain,level);
            }
        }
        /// we have to add the index of the vertex to the indexed representation of the index
        indexed_tri.push_back(ind);
    }
    else
    {
        for (int i = 0; i < this->subdivision.son_number(); i++)
        {
            Box son_dom = this->subdivision.compute_domain(domain,level,i);
            int son_level = level +1;
            if (son_dom.contains(v,this->mesh.get_domain().get_max()))
            {
                this->add_vertex_from_soup(*n.get_son(i),son_dom,son_level,v,vertex_index,indexed_tri,first_time);
            }
        }
    }
}

itype PRT_Tree::is_already_inserted(Node_V& n, Vertex &v)
{
    itype ind = -1;
    for(RunIteratorPair itPair = n.make_v_array_iterator_pair(); itPair.first != itPair.second; ++itPair.first)
    {
        Vertex &vert = mesh.get_vertex(*itPair.first);
        if(v == vert)
        {
            ind = *itPair.first;
            break;
        }
    }
    return ind;
}

void PRT_Tree::build_tree_from_cloud(vertex_multifield &multifield)
{
    for(int i=1;i<=this->mesh.get_vertices_num(); i++)
    {
        this->add_vertex_from_cloud(this->root,this->mesh.get_domain(),0,mesh.get_vertex(i),i,multifield);
    }
}

void PRT_Tree::add_vertex_from_cloud(Node_V &n, Box &domain, int level, Vertex &v, itype vertex_index, vertex_multifield &multifield)
{
    if (n.is_leaf())
    {
        int ind = this->is_already_inserted(n,v);
        if(ind != -1) /// already indexed
        {
            multifield[ind].insert(v.get_z());
            ///flag the vertex as removed
            mesh.remove_vertex(vertex_index);
        }
        else
        {
            /// we have to add the index of the vertex to the indexed representation of the index
            n.add_vertex(vertex_index);
            /// and then initialize the corresponding entry in multifield
            dset mf;
            mf.insert(v.get_z());
            multifield.insert(make_pair(vertex_index,mf));

            /// in case of split we call the "usual" addVertex because we have all we need into the mesh variable
            if (is_full(n))
            {
                this->split(n,domain,level);
            }
        }
    }
    else
    {
        for (int i = 0; i < this->subdivision.son_number(); i++)
        {
            Box son_dom = this->subdivision.compute_domain(domain,level,i);
            int son_level = level +1;
            if (son_dom.contains(v,this->mesh.get_domain().get_max()))
            {
                this->add_vertex_from_cloud(*n.get_son(i),son_dom,son_level,v,vertex_index,multifield);
            }
        }
    }
}
