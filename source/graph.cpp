#include "graph.h"

Graph::Graph()
{

}

Graph::~Graph()
{

}

void Graph::addFace(Facet facet)
{
    if(_facets.empty())
    {
        // first facet
        _facets.insert(std::make_pair(0, facet));
    }
    else
    {
        // all other facets get the index+1
        _facets.insert(std::make_pair(_facets.rbegin()->first+1 , facet));
    }
}

void Graph::calculateDual()
{
    // loop through all faces
    for(std::pair<int, Facet> facet : _facets)
    {
        int faceId = facet.first;

        // loop through halfedges of all faces and add the dual edges, that were not added yet
        // use distance as weight
        Polyhedron::Halfedge_around_facet_circulator hfc = facet.second->facet_begin();
        do
        {
            // get the opposing face
            int oppositeFaceId = getFacetID(hfc->opposite()->facet());

            // use distance as meassurement
            double distance = sqrt(CGAL::squared_distance(hfc->prev()->vertex()->point(), hfc->vertex()->point()));


            // center of the edge
            QVector3D center = QVector3D(float((hfc->prev()->vertex()->point().x() + hfc->vertex()->point().x())),
                                         float((hfc->prev()->vertex()->point().y() + hfc->vertex()->point().y())),
                                         float((hfc->prev()->vertex()->point().z() + hfc->vertex()->point().z()))) / 2;

            Edge edge = Edge(faceId, oppositeFaceId, distance, center, hfc);

            // if this edge doesn't exist already, add it (don't consider direction)
            if(!hasEdge(edge))
            {
                _edges.push_back(edge);
            }

        } while (++hfc != facet.second->facet_begin());
    }
}

void Graph::calculateMSP()
{
    // clear the previous msp edges
    _mspEdges.clear();
    _cutEdges.clear();

    // sort edges from smallest to biggest, better cut big ones
    std::sort(_edges.begin(), _edges.end());

    // create the adjacence list
    std::vector<std::vector<int>> adjacenceList;
    adjacenceList.resize(_facets.size());

    // go through all possible edges
    for(Edge& edge : _edges)
    {
        // add edge to msp, add adjacent faces
        _mspEdges.push_back(edge);
        adjacenceList[ulong(edge._sFace)].push_back(edge._tFace);
        adjacenceList[ulong(edge._tFace)].push_back(edge._sFace);

        // list storing discovered nodes
        std::vector<bool> discovered(_facets.size());

        // if the MSP is now cyclic, the added egde needs to be removed again
        for(ulong i = 0; i < _facets.size(); i++)
        {
            // if the node is alone (no incident edges), or already discovered, no need to check
            if(adjacenceList[i].empty() || discovered[i])
                continue;

            // if the graph is cyclic
            if(!isAcyclic(adjacenceList, i, discovered, -1))
            {
                _mspEdges.erase(remove(_mspEdges.begin(), _mspEdges.end(), edge), _mspEdges.end());

                // add edge to the "to be cut" list
                _cutEdges.push_back(edge);

                // cleanup the adjacence list
                adjacenceList[ulong(edge._sFace)].erase(remove(adjacenceList[ulong(edge._sFace)].begin(), adjacenceList[ulong(edge._sFace)].end(), edge._tFace), adjacenceList[ulong(edge._sFace)].end());
                adjacenceList[ulong(edge._tFace)].erase(remove(adjacenceList[ulong(edge._tFace)].begin(), adjacenceList[ulong(edge._tFace)].end(), edge._sFace), adjacenceList[ulong(edge._tFace)].end());

                // no need to continue checking
                break;
            }
        }
    }

#ifndef NDEBUG
    std::cout << "number of faces: " << _facets.size() << std::endl;
    std::cout << "number of edges: " << _mspEdges.size() << std::endl;

    // debugging purpose: show all edges of the MSP
    std::cout << "MSP over the edges" << std::endl;
    for(Edge& edge : _mspEdges)
        std::cout << "Edge: " << edge._sFace << "<->" << edge._tFace << std::endl;

    if(isSingleComponent(adjacenceList))
        std::cout << "Graph is a single component!" << std::endl;
    else
        std::cout << "Graph is a NOT single component!" << std::endl;
#endif
}

void Graph::calculateGlueTags(std::vector<QVector3D>& gtVertices, std::vector<GLushort>& gtIndices, std::vector<QVector3D>& gtColors)
{
    std::cout << "Gluetags: " << _cutEdges.size() << std::endl;
    std::cout << "Edges 'to be bent': " << _mspEdges.size() << std::endl;
    int index = int(gtVertices.size())-1;

    for(Edge& edge : _cutEdges)
    {
        QVector3D baseBL = QVector3D(float(edge._halfedge->vertex()->point().x()),
                                     float(edge._halfedge->vertex()->point().y()),
                                     float(edge._halfedge->vertex()->point().z()));

        QVector3D baseBR = QVector3D(float(edge._halfedge->prev()->vertex()->point().x()),
                                    float(edge._halfedge->prev()->vertex()->point().y()),
                                    float(edge._halfedge->prev()->vertex()->point().z()));

        QVector3D up = baseBR.normalized() / 4;
        QVector3D side = (baseBL - baseBR).normalized() / 4;

        QVector3D baseTL = baseBL + up - side;
        QVector3D baseTR = baseBR + up + side;

        gtVertices.push_back(baseBL); // bottom left = -3
        gtVertices.push_back(baseBR); // bottom right = -2
        gtVertices.push_back(baseTR); // top right = -1
        gtVertices.push_back(baseTL); // top left = 0

        index += 4;

        gtIndices.push_back(GLushort(index-3));
        gtIndices.push_back(GLushort(index-2));
        gtIndices.push_back(GLushort(index-1));
        gtIndices.push_back(GLushort(index-3));
        gtIndices.push_back(GLushort(index-1));
        gtIndices.push_back(GLushort(index));

        gtColors.push_back(QVector3D(0.2f, 0.2f, 0.8f));
        gtColors.push_back(QVector3D(0.2f, 0.2f, 0.8f));
        gtColors.push_back(QVector3D(0.2f, 0.2f, 0.8f));
        gtColors.push_back(QVector3D(0.2f, 0.2f, 0.8f));
        gtColors.push_back(QVector3D(0.2f, 0.2f, 0.8f));
        gtColors.push_back(QVector3D(0.2f, 0.2f, 0.8f));
    }
}

bool Graph::isSingleComponent(std::vector<std::vector<int>>& adjacenceList)
{
    // list storing discovered nodes
    std::vector<bool> discovered(_facets.size());

    bool isSingleComponent = true;

    // check if it is acyclic from the first node
    isSingleComponent = isAcyclic(adjacenceList, 0, discovered, -1);

    // if not all nodes have been discovered, the graph is not connected
    for (ulong i = 0; i < discovered.size(); i++)
    {
        if(!discovered[i])
        {
            isSingleComponent = false;
#ifndef NDEBUG
            std::cout << "not connected face: " << i << std::endl;
#endif
        }
    }
    return isSingleComponent;
}

bool Graph::isAcyclic(std::vector<std::vector<int>> const &adjacenceList, ulong start, std::vector<bool> &discovered, int parent)
{
    // mark current node as discovered
    discovered[start] = true;

    // loop through every edge from (start -> node(s))
    for(int node : adjacenceList[start])
    {
        // if this node was not discovered
        if (!discovered[ulong(node)])
        {
            if(!isAcyclic(adjacenceList, ulong(node), discovered, int(start))) // start dfs from node
                return false;
        }
        // node is discovered but not a parent => back-edge (cycle)
        else if (node != parent)
        {
            return false;
        }
    }

    // graph is acyclic
    return true;
}

void Graph::lines(std::vector<QVector3D>& lineVertices, std::vector<QVector3D>& lineColors)
{
    // loop through all edges
    for(Edge& edge : _mspEdges)
    {
        // source face (center)
        lineVertices.push_back(faceCenter(_facets[edge._sFace]));
        // to middle of the edge
        lineVertices.push_back(edge._middle);
        // middle of the edge
        lineVertices.push_back(edge._middle);
        // to target face (center)
        lineVertices.push_back(faceCenter(_facets[edge._tFace]));

        lineColors.push_back(QVector3D(0.2f, 0.8f, 0.2f));
        lineColors.push_back(QVector3D(0.2f, 0.8f, 0.2f));
        lineColors.push_back(QVector3D(0.2f, 0.8f, 0.2f));
        lineColors.push_back(QVector3D(0.2f, 0.8f, 0.2f));
    }

    // add all cut edges
    for(Edge& edge : _cutEdges)
    {
        lineVertices.push_back(QVector3D(float(edge._halfedge->prev()->vertex()->point().x()),
                                        float(edge._halfedge->prev()->vertex()->point().y()),
                                        float(edge._halfedge->prev()->vertex()->point().z())));
        lineVertices.push_back(QVector3D(float(edge._halfedge->vertex()->point().x()),
                                        float(edge._halfedge->vertex()->point().y()),
                                        float(edge._halfedge->vertex()->point().z())));

        lineColors.push_back(QVector3D(0.8f, 0.2f, 0.2f));
        lineColors.push_back(QVector3D(0.8f, 0.2f, 0.2f));
    }

    // add all edges that are not cut edges
    for(Edge& edge : _edges)
    {
        if(std::find(_cutEdges.begin(), _cutEdges.end(), edge) == _cutEdges.end())
        {
            lineVertices.push_back(QVector3D(float(edge._halfedge->prev()->vertex()->point().x()),
                                            float(edge._halfedge->prev()->vertex()->point().y()),
                                            float(edge._halfedge->prev()->vertex()->point().z())));
            lineVertices.push_back(QVector3D(float(edge._halfedge->vertex()->point().x()),
                                            float(edge._halfedge->vertex()->point().y()),
                                            float(edge._halfedge->vertex()->point().z())));

            lineColors.push_back(QVector3D(0.0f, 0.0f, 0.0f));
            lineColors.push_back(QVector3D(0.0f, 0.0f, 0.0f));
        }
    }
}

QVector3D Graph::faceCenter(Facet facet)
{
    QVector3D middle(0,0,0);

    // add all vertices of the face
    Polyhedron::Halfedge_around_facet_circulator hfc = facet->facet_begin();
    do
    {
        middle += QVector3D(float(hfc->vertex()->point().x()), float(hfc->vertex()->point().y()), float(hfc->vertex()->point().z()));
    } while (++hfc != facet->facet_begin());

    // divide by 3 to get the center
    middle /= 3.0f;

    return middle;
}

int Graph::getFacetID(Facet facet)
{
    // loop through all facets
    for(std::pair<int, Facet> pfacet : _facets)
    {
        // if saved facet is the same, return the index
        if(pfacet.second == facet)
        {
            return pfacet.first;
        }
    }
    // if this facet has no match return
    return -1;
}

bool Graph::hasEdge(Edge& edge)
{
    // loop through all edges
    for(Edge& pedge : _edges)
    {
        // return true if this edge was added to the graph
        if(pedge == edge)
        {
            return true;
        }
    }

    return false;
}
