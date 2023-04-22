#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <numeric>
#include <cassert>

#include "util.h"
#include "Steiner.h"

using namespace std;

inline void split(string &str, const string &delim, vector<string> &v) {
    size_t pos = 0;
    string token;
    v.clear();
    while ((pos = str.find(delim)) != string::npos) {
        token = str.substr(0, pos);
        str.erase(0, pos + delim.size());
        v.push_back(token);
    }
    v.push_back(str);
}

inline Point string2Point(string str) {
    vector<string> tokens;
    split(str, ",", tokens);
    int x = stoi(tokens[0].substr(1));
    tokens[1].pop_back();
    int y = stoi(tokens[1]);
    return Point(x, y);
}

inline string getFileName(const string &filePathName, bool getFile) {
    string retStr = filePathName;
    string::size_type pos = retStr.rfind("/");
    if (pos != string::npos) {
        if (getFile) retStr = retStr.substr(pos + 1);
        else retStr = retStr.substr(0, pos);
    }
    return retStr;
}

void Steiner::parse(const string &fileName) {
    _name = getFileName(fileName, true);
    ifstream ifs(fileName, ifstream::in);
    string buf;
    Point p;
    ifs >> buf >> buf >> buf;
    buf.pop_back();
    p = string2Point(buf);
    _boundaryLeft = p.x;
    _boundaryBottom = p.y;
    ifs >> buf;
    p = string2Point(buf);
    _boundaryRight = p.x;
    _boundaryTop = p.y;

    ifs >> buf >> buf >> buf;
    _points.resize(stoi(buf));
    _init_p = _points.size();
    for (unsigned i = 0; i < _points.size(); ++i) {
        ifs >> buf >> buf >> buf;
        _points[i] = string2Point(buf);
    }
    ifs.close();
}

void Steiner::init() {
    _edges.clear();
    _set.clear();
    _MST.clear();
    _edges_del.clear();
    _lca_place.clear();
    _lca_queries.clear();
    _lca_answer_queries.clear();
    _visit.clear();
    _ancestor.clear();
    _par.clear();
    _rank.clear();
    _newE.clear();
    _table.clear();
    _table_cnt.clear();
}

extern bool gDoplot;

void Steiner::solve() {
#ifdef VERBOSE
    TimeUsage timer;
#endif
    buildRSG();
#ifdef VERBOSE
    timer.showUsage("buildRSG", TimeUsage::PARTIAL);
    timer.start(TimeUsage::PARTIAL);
#endif
    buildMST();
#ifdef VERBOSE
    timer.showUsage("buildMST", TimeUsage::PARTIAL);
    timer.start(TimeUsage::PARTIAL);
#endif
    if (gDoplot) {
        _init_edges = _edges;
        _init_MST = _MST;
    }
    for (int eId: _MST) _MST_cost += _edges[eId].weight;
    buildRST();
#ifdef VERBOSE
    timer.showUsage("buildRST", TimeUsage::PARTIAL);
    timer.start(TimeUsage::PARTIAL);
#endif
    unsigned numIter = 2;
    if (_init_p >= 10) numIter = 2;
    if (_init_p >= 100) numIter = 3;
    if (_init_p >= 500) numIter = 4;
    for (unsigned iter = 1; iter < numIter; ++iter) {
        init();
        buildRSG();
#ifdef VERBOSE
        timer.showUsage("buildRSG", TimeUsage::PARTIAL);
        timer.start(TimeUsage::PARTIAL);
#endif
        buildMST();
#ifdef VERBOSE
        timer.showUsage("buildMST", TimeUsage::PARTIAL);
        timer.start(TimeUsage::PARTIAL);
#endif
        buildRST();
#ifdef VERBOSE
        timer.showUsage("buildRST", TimeUsage::PARTIAL);
        timer.start(TimeUsage::PARTIAL);
#endif
    }
    for (auto &e: _newE) _MRST_cost += e.weight;
    for (int eId: _MST) {
        if (_edges_del[eId]) continue;
        _MRST_cost += _edges[eId].weight;
    }
    cerr << _name << endl;
    cerr << "RSG edge   : " << _edges.size() << endl;
    cerr << "MST length : " << _MST_cost << endl;
    cerr << "MRST length: " << _MRST_cost << endl;
    cerr << "Improvement: "
         << (double) (_MST_cost - _MRST_cost) / _MST_cost * 100
         << "%" << endl;
}

void Steiner::addEdge(int p1, int p2) {
    if (p1 == p2) return;
    int weight = abs(_points[p1].x - _points[p2].x) +
                 abs(_points[p1].y - _points[p2].y);
    _edges.emplace_back(Edge(p1, p2, weight));
    _points[p1].neighbors.emplace_back(p2);
    _points[p2].neighbors.emplace_back(p1);
}

void Steiner::buildRSG() {
    vector<int> order1, order2;
    order1.resize(_points.size());
    iota(order1.begin(), order1.end(), 0);
    order2 = order1;
    sort(order1.begin(), order1.end(),
         [&](int i1, int i2) {
             return _points[i1].x + _points[i1].y < _points[i2].x + _points[i2].y;
         });
    sort(order2.begin(), order2.end(),
         [&](int i1, int i2) {
             return _points[i1].x - _points[i1].y < _points[i2].x - _points[i2].y;
         });
    vector<int> A1, A2;
    A1.reserve(_points.size());
    A2.reserve(_points.size());
    for (int pId: order1) {
        Point &p = _points[pId];
        if (!A1.empty()) {
            auto it = A1.end();
            do {
                --it;
                Point &tmp = _points[*it];
                if (p.y - tmp.y >= p.x - tmp.x &&
                    p.y - tmp.y > 0 &&
                    p.x - tmp.x > 0) {
                    addEdge(pId, *it);
                    it = A1.erase(it);
                    //break;
                }
            } while (it != A1.begin());
        }
        if (!A2.empty()) {
            auto it = A2.end();
            do {
                --it;
                Point &tmp = _points[*it];
                if (p.y - tmp.y < p.x - tmp.x &&
                    p.y - tmp.y >= 0 &&
                    p.x - tmp.x > 0) {
                    addEdge(pId, *it);
                    it = A2.erase(it);
                    //break;
                }
            } while (it != A2.begin());
        }
        A1.emplace_back(pId);
        A2.emplace_back(pId);
    }
    A1.clear();
    A2.clear();
    for (int pId: order2) {
        Point &p = _points[pId];
        if (!A1.empty()) {
            auto it = A1.end();
            do {
                --it;
                Point &tmp = _points[*it];
                if (tmp.y - p.y <= p.x - tmp.x &&
                    p.y - tmp.y < 0 &&
                    p.x - tmp.x > 0) {
                    addEdge(pId, *it);
                    it = A1.erase(it);
                    //break;
                }
            } while (it != A1.begin());
        }
        if (!A2.empty()) {
            auto it = A2.end();
            do {
                --it;
                Point &tmp = _points[*it];
                if (tmp.y - p.y > p.x - tmp.x &&
                    p.y - tmp.y < 0 &&
                    p.x - tmp.x >= 0) {
                    addEdge(pId, *it);
                    it = A2.erase(it);
                    //break;
                }
            } while (it != A2.begin());
        }
        A1.emplace_back(pId);
        A2.emplace_back(pId);
    }

}

unsigned Steiner::findSet(int x) {
    return x == _set[x] ? _set[x] : (_set[x] = findSet(_set[x]));
}

void Steiner::unionSet(int x, int y, int z) {
    int xroot = findSet(x);
    int yroot = findSet(y);
    assert (xroot != yroot);
    _set[xroot] = z;
    _set[yroot] = z;
}

void Steiner::buildMST() {
    sort(_edges.begin(), _edges.end(),
         [](Edge e1, Edge e2) {
             return e1.weight < e2.weight;
         });
    _set.resize(_edges.size() + _points.size());
    iota(_set.begin(), _set.end(), 0);
    _lca_place.resize(_points.size());
    for (unsigned i = 0; i < _edges.size(); ++i) {
        Edge &e = _edges[i];
        unsigned head1 = findSet(e.p1 + _edges.size());
        unsigned head2 = findSet(e.p2 + _edges.size());
        if (head1 != head2) {
            set<int> neighbors;
            for (int n: _points[e.p1].neighbors) neighbors.emplace(n);
            for (int n: _points[e.p2].neighbors) neighbors.emplace(n);
            neighbors.erase(e.p1);
            neighbors.erase(e.p2);
            for (int w: neighbors) {
                if (head1 == findSet(w + _edges.size())) {
                    _lca_place[w].emplace_back(_lca_queries.size());
                    _lca_place[e.p1].emplace_back(_lca_queries.size());
                    _lca_queries.emplace_back(w, e.p1, i);
                } else {
                    _lca_place[w].emplace_back(_lca_queries.size());
                    _lca_place[e.p2].emplace_back(_lca_queries.size());
                    _lca_queries.emplace_back(w, e.p2, i);
                }
            }
            unionSet(head1, head2, i);
            e.left = head1;
            e.right = head2;
            _MST.emplace_back(i);
        }
    }
    _root = findSet(0);
}

int Steiner::tarfind(int x) {
    return x == _par[x] ? _par[x] : (_par[x] = tarfind(_par[x]));
}

void Steiner::tarunion(int x, int y) {
    int xroot = tarfind(x);
    int yroot = tarfind(y);
    if (_rank[xroot] > _rank[yroot]) _par[yroot] = xroot;
    else if (_rank[xroot] < _rank[yroot]) _par[xroot] = yroot;
    else {
        _par[yroot] = xroot;
        ++_rank[xroot];
    }
}

void Steiner::tarjanLCA(int x) {
    _par[x] = x;
    _ancestor[x] = x;
    if (x < (int) _edges.size()) {
        tarjanLCA(_edges[x].left);
        tarunion(x, _edges[x].left);
        _ancestor[tarfind(x)] = x;
        tarjanLCA(_edges[x].right);
        tarunion(x, _edges[x].right);
        _ancestor[tarfind(x)] = x;
    }
    _visit[x] = true;
    if (x >= (int) _edges.size()) {
        int u = x - _edges.size();
        for (unsigned i = 0; i < _lca_place[u].size(); ++i) {
            int which = _lca_place[u][i];
            int v = get<0>(_lca_queries[which]) == u ?
                    get<1>(_lca_queries[which]) : get<0>(_lca_queries[which]);
            v += _edges.size();
            if (_visit[v]) _lca_answer_queries[which] = _ancestor[tarfind(v)];
        }
    }
}

void Steiner::buildRST() {
    _lca_answer_queries.resize(_lca_queries.size());
    _visit.resize(_edges.size() + _points.size());
    _ancestor.resize(_edges.size() + _points.size());
    _par.resize(_edges.size() + _points.size());
    _rank.resize(_edges.size() + _points.size());
    tarjanLCA(_root);
    _table.reserve(_lca_queries.size());
    _table_cnt.resize(_edges.size());
    for (unsigned i = 0; i < _lca_queries.size(); ++i) {
        int p = get<0>(_lca_queries[i]);
        int ae = get<2>(_lca_queries[i]);
        int de = _lca_answer_queries[i];
        Point &pnt = _points[p];
        Edge &add_e = _edges[ae];
        Edge &del_e = _edges[de];
        int gain = del_e.weight;
        int mxx = max(_points[add_e.p1].x, _points[add_e.p2].x);
        int mnx = min(_points[add_e.p1].x, _points[add_e.p2].x);
        int mxy = max(_points[add_e.p1].y, _points[add_e.p2].y);
        int mny = min(_points[add_e.p1].y, _points[add_e.p2].y);
        if (pnt.x < mnx) gain -= mnx - pnt.x;
        else if (pnt.x > mxx) gain -= pnt.x - mxx;
        if (pnt.y < mny) gain -= mny - pnt.y;
        else if (pnt.y > mxy) gain -= pnt.y - mxy;
        if (gain > 0) {
            ++_table_cnt[ae];
            ++_table_cnt[de];
            _table.emplace_back(p, ae, de, gain);
        }
    }
    auto sortByGain = [&](tuple<int, int, int, int> t1,
                          tuple<int, int, int, int> t2) {
        if (get<3>(t1) == get<3>(t2)) {
            return _table_cnt[get<1>(t1)] + _table_cnt[get<2>(t1)] <
                   _table_cnt[get<1>(t2)] + _table_cnt[get<2>(t2)];
        }
        return get<3>(t1) > get<3>(t2);
    };
    sort(_table.begin(), _table.end(), sortByGain);
#ifdef DEBUG
    for (unsigned i = 0; i < _table.size(); ++i) {
      cerr << get<0>(_table[i]) << " (" << _edges[get<1>(_table[i])].p1 << ","
           << _edges[get<1>(_table[i])].p2 << ")"
           << " (" << _edges[get<2>(_table[i])].p1 << ","
           << _edges[get<2>(_table[i])].p2 << ") "
           << get<3>(_table[i]) << endl;
    }
#endif
    _edges_del.resize(_edges.size());
    for (unsigned i = 0; i < _table.size(); ++i) {
        int ae = get<1>(_table[i]);
        int de = get<2>(_table[i]);
        if (_edges_del[ae] || _edges_del[de]) continue;
        Point p = _points[get<0>(_table[i])];
        Edge &add_e = _edges[ae];
        int mxx = max(_points[add_e.p1].x, _points[add_e.p2].x);
        int mnx = min(_points[add_e.p1].x, _points[add_e.p2].x);
        int mxy = max(_points[add_e.p1].y, _points[add_e.p2].y);
        int mny = min(_points[add_e.p1].y, _points[add_e.p2].y);
        int sx = p.x, sy = p.y;
        if (p.x < mnx) sx = mnx;
        else if (p.x > mxx) sx = mxx;
        if (p.y < mny) sy = mny;
        else if (p.y > mxy) sy = mxy;
        if (sx != p.x || sy != p.y) {
            int pId = get<0>(_table[i]);
            int new_pId = _points.size();
            Point new_p = Point(sx, sy);
            _points.emplace_back(new_p);
            int weight = abs(p.x - new_p.x) + abs(p.y - new_p.y);
            int weight1 = abs(_points[add_e.p1].x - new_p.x) +
                          abs(_points[add_e.p1].y - new_p.y);
            int weight2 = abs(_points[add_e.p2].x - new_p.x) +
                          abs(_points[add_e.p2].y - new_p.y);
            _newE.emplace_back(Edge(new_pId, pId, weight));
            _newE.emplace_back(Edge(new_pId, add_e.p1, weight1));
            _newE.emplace_back(Edge(new_pId, add_e.p2, weight2));
        }
        _edges_del[ae] = true;
        _edges_del[de] = true;
    }

}

void Steiner::plot(const string &plotName) {
    string ofileName = plotName;
    ofstream of(ofileName, ofstream::out);
    of << "set size ratio -1" << endl;
    of << "set nokey" << endl;
    of << "set xrange["
       << (_boundaryRight - _boundaryLeft) * -0.05 << ":"
       << (_boundaryRight - _boundaryLeft) * 1.05 << "]" << endl;
    of << "set yrange["
       << (_boundaryTop - _boundaryBottom) * -0.05 << ":"
       << (_boundaryTop - _boundaryBottom) * 1.05 << "]" << endl;
    int idx = 1;
    of << "set object " << idx++ << " rect from "
       << _boundaryLeft << "," << _boundaryBottom << " to "
       << _boundaryRight << "," << _boundaryTop << "fc rgb \"grey\" behind\n";
    // point
    for (int i = 0; i < _init_p; ++i) {
        of << "set object circle at first " << _points[i].x << ","
           << _points[i].y << " radius char 0.3 fillstyle solid "
           << "fc rgb \"red\" front\n";
    }
    // RSG
    for (unsigned i = 0; i < _init_edges.size(); ++i) {
        Point &p1 = _points[_init_edges[i].p1];
        Point &p2 = _points[_init_edges[i].p2];
        of << "set arrow " << idx++ << " from "
           << p1.x << "," << p1.y << " to "
           << p2.x << "," << p2.y
           << " nohead lc rgb \"white\" lw 1 front\n";
    }
    // MST
    for (unsigned i = 0; i < _init_MST.size(); ++i) {
        Point &p1 = _points[_init_edges[_init_MST[i]].p1];
        Point &p2 = _points[_init_edges[_init_MST[i]].p2];
        of << "set arrow " << idx++ << " from "
           << p1.x << "," << p1.y << " to "
           << p2.x << "," << p2.y
           << " nohead lc rgb \"blue\" lw 1 front\n";
    }
    // s-point
    for (unsigned i = _init_p; i < _points.size(); ++i) {
        of << "set object circle at first " << _points[i].x << ","
           << _points[i].y << " radius char 0.3 fillstyle solid "
           << "fc rgb \"yellow\" front\n";
    }
    // RST
    for (unsigned i = 0; i < _MST.size(); ++i) {
        if (_edges_del[_MST[i]]) continue;
        Point &p1 = _points[_edges[_MST[i]].p1];
        Point &p2 = _points[_edges[_MST[i]].p2];
        if (p1.x != p2.x) {
            of << "set arrow " << idx++ << " from "
               << p1.x << "," << p1.y << " to "
               << p2.x << "," << p1.y
               << " nohead lc rgb \"black\" lw 1.5 front\n";
        }
        if (p1.y != p2.y) {
            of << "set arrow " << idx++ << " from "
               << p2.x << "," << p1.y << " to "
               << p2.x << "," << p2.y
               << " nohead lc rgb \"black\" lw 1.5 front\n";
        }
    }
    for (unsigned i = 0; i < _newE.size(); ++i) {
        of << "set arrow " << idx++ << " from "
           << _points[_newE[i].p1].x << "," << _points[_newE[i].p1].y << " to "
           << _points[_newE[i].p2].x << "," << _points[_newE[i].p1].y
           << " nohead lc rgb \"black\" lw 1.5 front\n";
        of << "set arrow " << idx++ << " from "
           << _points[_newE[i].p2].x << "," << _points[_newE[i].p1].y << " to "
           << _points[_newE[i].p2].x << "," << _points[_newE[i].p2].y
           << " nohead lc rgb \"black\" lw 1.5 front\n";
    }
    of << "plot 1000000000" << endl;
    of << "pause -1 'Press any key'" << endl;
    of.close();
}

void Steiner::outfile(const string &outfileName) {
    ofstream of(outfileName, ofstream::out);
    of << "NumRoutedPins = " << _init_p << endl;
    of << "WireLength = " << _MRST_cost << endl;
    for (unsigned i = 0; i < _MST.size(); ++i) {
        if (_edges_del[_MST[i]]) continue;
        Point &p1 = _points[_edges[_MST[i]].p1];
        Point &p2 = _points[_edges[_MST[i]].p2];
        if (p1.x != p2.x) {
            of << "H-line "
               << "(" << p1.x << "," << p1.y << ") "
               << "(" << p2.x << "," << p1.y << ")"
               << endl;
        }
        if (p1.y != p2.y) {
            of << "V-line "
               << "(" << p2.x << "," << p1.y << ") "
               << "(" << p2.x << "," << p2.y << ")"
               << endl;
        }
    }
    for (unsigned i = 0; i < _newE.size(); ++i) {
        Point &p1 = _points[_newE[i].p1];
        Point &p2 = _points[_newE[i].p2];
        if (p1.x != p2.x) {
            of << "H-line "
               << "(" << p1.x << "," << p1.y << ") "
               << "(" << p2.x << "," << p1.y << ")"
               << endl;
        }
        if (p1.y != p2.y) {
            of << "V-line "
               << "(" << p2.x << "," << p1.y << ") "
               << "(" << p2.x << "," << p2.y << ")"
               << endl;
        }
    }
    of.close();
}

//Added Code
void Steiner::createSteiner(const std::string &fileName, std::vector<Point> Nets, Boundary Bounds) {
    _name = getFileName(fileName, true);
    _boundaryLeft = Bounds.xleft;
    _boundaryRight = Bounds.xright;
    _boundaryTop = Bounds.ytop;
    _boundaryBottom = Bounds.ybot;

    _points.resize(Nets.size());
    for (int i = 0; i < Nets.size(); i++) {
        _points[i] = Nets.at(i);
    }
    _init_p = _points.size();
}

int Steiner::initializeFile(std::ofstream &of) {
    of << "set size ratio -1" << endl;
    of << "set nokey" << endl;
    of << "set xrange["
       << (_boundaryRight - _boundaryLeft) * -0.05 << ":"
       << (_boundaryRight - _boundaryLeft) * 1.05 << "]" << endl;
    of << "set yrange["
       << (_boundaryTop - _boundaryBottom) * -0.05 << ":"
       << (_boundaryTop - _boundaryBottom) * 1.05 << "]" << endl;
    int idx = 1;
    of << "set object " << idx++ << " rect from "
       << _boundaryLeft << "," << _boundaryBottom << " to "
       << _boundaryRight << "," << _boundaryTop << "fc rgb \"grey\" behind\n";
    return idx;
}

int Steiner::plotMultiple(std::ofstream &file, int idx, std::vector<std::vector<std::vector<int>>> &horizontal, std::vector<std::vector<std::vector<int>>> &vertical, std::string color) {
    // point
    for (int i = 0; i < _init_p; ++i) {

        file << "set object circle at first " << _points[i].x << ","
             << _points[i].y << " radius char 0.3 fillstyle solid "
             << "fc rgb \"black\" front\n";
        file << "set object circle at first " << _points[i].x << ","
             << _points[i].y << " radius char 0.15 fillstyle solid "
             << "fc rgb \""
             << color
             << "\" front\n";
    }
    // RSG
    for (unsigned i = 0; i < _init_edges.size(); ++i) {
        Point &p1 = _points[_init_edges[i].p1];
        Point &p2 = _points[_init_edges[i].p2];
        // file << "set arrow " << idx++ << " from "
        // << p1.x << "," << p1.y << " to "
        // << p2.x << "," << p2.y
        // << " nohead lc rgb \"white\" lw 1 front\n";
    }
    // MST
    for (unsigned i = 0; i < _init_MST.size(); ++i) {
        Point &p1 = _points[_init_edges[_init_MST[i]].p1];
        Point &p2 = _points[_init_edges[_init_MST[i]].p2];
        // file << "set arrow " << idx++ << " from "
        // << p1.x << "," << p1.y << " to "
        // << p2.x << "," << p2.y
        // << " nohead lc rgb \"blue\" lw 1 front\n";
    }
    // s-point
    for (unsigned i = _init_p; i < _points.size(); ++i) {
        file << "set object circle at first " << _points[i].x << ","
             << _points[i].y << " radius char 0.3 fillstyle solid "
             << "fc rgb \"yellow\" front\n";
    }
    std::vector<std::vector<int>> horizontalNet;
    std::vector<std::vector<int>> verticalNet;
    // RST
    for (unsigned i = 0; i < _MST.size(); ++i) {
        //if (_edges_del[_MST[i]]) continue;
        Point &p1 = _points[_edges[_MST[i]].p1];
        Point &p2 = _points[_edges[_MST[i]].p2];
        if (p1.x != p2.x) {
            std::vector<int> tempH;
            file << "set arrow " << idx++ << " from "
                 << p1.x << "," << p1.y << " to "
                 << p2.x << "," << p1.y
                 << " nohead lc rgb \""
                 << color
                 << "\" lw 1.5 back\n";
            //checkEdges << "H " << p1.y << " " << p1.x << " " << p2.x << std::endl;
            tempH.push_back(p1.y);
            if (p1.x > p2.x) {
                tempH.push_back(p2.x);
                tempH.push_back(p1.x);
            } else {
                tempH.push_back(p1.x);
                tempH.push_back(p2.x);
            }
            horizontalNet.push_back(tempH);
        }
        if (p1.y != p2.y) {
            std::vector<int> tempV;
            file << "set arrow " << idx++ << " from "
                 << p2.x << "," << p1.y << " to "
                 << p2.x << "," << p2.y
                 << " nohead lc rgb \""
                 << color
                 << "\" lw 1.5 back\n";
            // checkEdges << "V " << p2.x << " " << p1.y << " " << p2.y << std::endl;
            tempV.push_back(p2.x);
            if (p1.y > p2.y) {
                tempV.push_back(p2.y);
                tempV.push_back(p1.y);
            } else {
                tempV.push_back(p1.y);
                tempV.push_back(p2.y);
            }
            verticalNet.push_back(tempV);
        }
    }
    horizontal.push_back(horizontalNet);
    vertical.push_back(verticalNet);
    return idx;
}

std::vector<Point> Steiner::getPoints() {
    return _points;
} //needs a getter
std::vector<Edge> Steiner::getEdges() {
    return _edges;
}        //needs a getter
std::vector<int> Steiner::getMST() {
    return _MST;
}        //needs a getter
std::vector<bool> Steiner::getEdges_del() {
    return _edges_del;
}    //neeeds a getter
void checkNets(std::ofstream &file, std::vector<std::vector<Reroute>> &errors, std::vector<std::vector<std::vector<int>>> horizontal, std::vector<std::vector<std::vector<int>>> vertical) {
    for (int a = 0; a < horizontal.size(); a++) {
        for (int i = 0; i < horizontal[a].size(); i++) {
            int yAxis = horizontal[a][i].at(0);
            int xMax = horizontal[a][i].at(1);
            int xMin = horizontal[a][i].at(2);
            if (xMin > xMax) {
                xMin = xMax;
                xMax = horizontal[a][i].at(2);
            }
            for (int j = 0; j < vertical.size(); j++) {
                if (j != a) {
                    for (int k = 0; k < vertical[j].size(); k++) {
                        int xAxis = vertical[j][k].at(0);
                        int yMax = vertical[j][k].at(1);
                        int yMin = vertical[j][k].at(2);
                        if (yMin > yMax) {
                            yMin = yMax;
                            yMax = vertical[j][k].at(2);
                        }
                        if (((yAxis >= yMin) & (yAxis <= yMax)) & ((xAxis >= xMin) & (xAxis <= xMax))) {
                            file << "set object circle at first " << xAxis << ","
                                 << yAxis << " radius char 0.3 fillstyle solid "
                                 << "fc rgb \"red\" front\n";
                            std::vector<Reroute> temp;
                            Reroute left = Reroute(a, 0, j, xAxis, xMin, xMax, yAxis, yAxis);
                            Reroute right = Reroute(a, 0, j, xAxis, xMax, xMin, yAxis, yAxis);
                            Reroute bottom = Reroute(j, 1, a, yAxis, yMin, yMax, xAxis, xAxis);
                            Reroute top = Reroute(j, 1, a, yAxis, yMax, yMin, xAxis, xAxis);

                            temp.push_back(left);
                            temp.push_back(right);
                            temp.push_back(bottom);
                            temp.push_back(top);
                            errors.push_back(temp);

                        }
                    }
                }
            }
        }
    }
}

int Steiner::plotFixed(std::ofstream &file, int idx, std::vector<std::vector<std::vector<int>>> &horizontal, std::vector<std::vector<std::vector<int>>> &vertical, std::vector<std::string> color, int initialColor) {
    // point
    for (int i = 0; i < _init_p; ++i) {

        file << "set object circle at first " << _points[i].x << ","
             << _points[i].y << " radius char 0.3 fillstyle solid "
             << "fc rgb \"black\" front\n";
        file << "set object circle at first " << _points[i].x << ","
             << _points[i].y << " radius char 0.15 fillstyle solid "
             << "fc rgb \""
             << color.at(initialColor)
             << "\" front\n";
    }
    // RSG
    for (unsigned i = 0; i < _init_edges.size(); ++i) {
        Point &p1 = _points[_init_edges[i].p1];
        Point &p2 = _points[_init_edges[i].p2];
        // file << "set arrow " << idx++ << " from "
        // << p1.x << "," << p1.y << " to "
        // << p2.x << "," << p2.y
        // << " nohead lc rgb \"white\" lw 1 front\n";
    }
    // MST
    for (unsigned i = 0; i < _init_MST.size(); ++i) {
        Point &p1 = _points[_init_edges[_init_MST[i]].p1];
        Point &p2 = _points[_init_edges[_init_MST[i]].p2];
        // file << "set arrow " << idx++ << " from "
        // << p1.x << "," << p1.y << " to "
        // << p2.x << "," << p2.y
        // << " nohead lc rgb \"blue\" lw 1 front\n";
    }
    // s-point
    for (unsigned i = _init_p; i < _points.size(); ++i) {
        // file << "set object circle at first " << _points[i].x << ","
        // << _points[i].y << " radius char 0.3 fillstyle solid "
        // << "fc rgb \"yellow\" front\n";
    }
    // RST
    if (!initialColor) {
        for (unsigned i = 0; i < horizontal.size(); ++i) {
            for (unsigned j = 0; j < horizontal[i].size(); ++j) {
                file << "set arrow " << idx++ << " from "
                     << horizontal[i][j].at(1) << "," << horizontal[i][j].at(0) << " to "
                     << horizontal[i][j].at(2) << "," << horizontal[i][j].at(0)
                     << " nohead lc rgb \""
                     << color.at(i % color.size())
                     << "\" lw 1.5 back\n";
            }
        }
        for (unsigned i = 0; i < vertical.size(); ++i) {
            for (unsigned j = 0; j < vertical[i].size(); ++j) {
                file << "set arrow " << idx++ << " from "
                     << vertical[i][j].at(0) << "," << vertical[i][j].at(1) << " to "
                     << vertical[i][j].at(0) << "," << vertical[i][j].at(2)
                     << " nohead lc rgb \""
                     << color.at(i % color.size())
                     << "\" lw 1.5 back\n";
            }
        }
    }
    return idx;
}

void fixError(std::vector<std::vector<Reroute>> &errors, std::vector<std::vector<std::vector<int>>> &horizontal, std::vector<std::vector<std::vector<int>>> &vertical) {
    int buffer = 2;

    while (!errors.empty()) {
        // std::cout << errors.size() << std::endl;
        int min = 4000;
        int minIdx = 0;
        int netIdx = 0;
        Reroute distance;
        int verticalIdx;
        int verticalNetNum;
        int horizontalIdx;
        int horizontalNetNum;
        for (int i = 0; i < errors[0].size(); i++) {
            Reroute candidate = errors[0].at(i);
            if (candidate.isV) {
                verticalIdx = vertical[candidate.netNum].size() + 1;
                for (int j = 0; j < vertical[candidate.netNum].size(); j++) {
                    if (i % 2 == 0) {
                        if ((candidate.head == vertical[candidate.netNum][j].at(1)) & (candidate.tail == vertical[candidate.netNum][j].at(2)) & (candidate.lWing == vertical[candidate.netNum][j].at(0))) {
                            verticalIdx = j;
                            verticalNetNum = candidate.netNum;
                            // std::cout << "found bottom at " << verticalIdx << std::endl;
                            // std::cout << vertical[candidate.netNum][j].at(0) << std::endl;
                            // std::cout << vertical[candidate.netNum][j].at(1) << std::endl;
                            // std::cout << vertical[candidate.netNum][j].at(2) << std::endl;
                        }
                    } else {
                        if ((candidate.head == vertical[candidate.netNum][j].at(2)) & (candidate.tail == vertical[candidate.netNum][j].at(1)) & (candidate.lWing == vertical[candidate.netNum][j].at(0))) {
                            verticalIdx = j;
                            verticalNetNum = candidate.netNum;
                            // std::cout << "found top at " << verticalIdx << std::endl;
                            // std::cout << vertical[candidate.netNum][j].at(0) << std::endl;
                            // std::cout << vertical[candidate.netNum][j].at(1) << std::endl;
                            // std::cout << vertical[candidate.netNum][j].at(2) << std::endl;
                        }
                    }
                }
                // std::cout << verticalIdx << std::endl;
                if (verticalIdx < vertical[candidate.netNum].size()) {
                    errors[0].at(i).head = candidate.head + (((i % 2) * 2 - 1) * buffer);
                    errors[0].at(i).lWing = candidate.lWing + (((i % 2) * 2 - 1) * buffer);
                    errors[0].at(i).rWing = candidate.rWing - (((i % 2) * 2 - 1) * buffer);
                    // std::cout << errors[0].at(i).head << " " << errors[0].at(i).lWing << " " << errors[0].at(i).rWing << std::endl;
                    bool verticalHead = true;
                    bool verticalWing = true;
                    while (!(errors[0].at(i) == candidate) | verticalHead | verticalWing) {
                        verticalHead = false;
                        verticalWing = false;
                        candidate = errors[0].at(i);
                        for (int m = 0; m < vertical[candidate.netNum].size(); m++) {
                            int smallNum = vertical[candidate.netNum][m][1];
                            int bigNum = vertical[candidate.netNum][m][2];
                            if ((smallNum <= candidate.head) & (bigNum >= candidate.head)) {
                                if (i % 2 == 0) {
                                    if ((vertical[candidate.netNum][m][0] >= candidate.lWing) && (vertical[candidate.netNum][m][0] <= candidate.rWing)) {
                                        errors[0].at(i).head = smallNum - buffer;
                                        verticalHead = true;
                                    }
                                } else {
                                    if ((vertical[candidate.netNum][m][0] >= candidate.rWing) && (vertical[candidate.netNum][m][0] <= candidate.lWing)) {
                                        errors[0].at(i).head = bigNum + buffer;
                                        verticalHead = true;
                                    }
                                }
                            }
                        }
                        candidate = errors[0].at(i);
                        for (int n = 0; n < horizontal[candidate.netNum].size(); n++) {
                            int smallNum = horizontal[candidate.netNum][n][1];
                            int bigNum = horizontal[candidate.netNum][n][2];
                            if (i % 2 == 0) {
                                if ((horizontal[candidate.netNum][n][0] >= candidate.head) & (horizontal[candidate.netNum][n][0] <= candidate.issuePoint)) {
                                    if ((smallNum <= candidate.lWing) & (bigNum >= candidate.lWing)) {
                                        errors[0].at(i).lWing = smallNum - buffer;
                                        verticalWing = true;
                                    }
                                    if ((smallNum <= candidate.rWing) & (bigNum >= candidate.rWing)) {
                                        errors[0].at(i).rWing = bigNum + buffer;
                                        verticalWing = true;
                                    }
                                }
                            } else {
                                if ((horizontal[candidate.netNum][n][0] <= candidate.head) & (horizontal[candidate.netNum][n][0] >= candidate.issuePoint)) {
                                    if ((smallNum <= candidate.lWing) & (bigNum >= candidate.lWing)) {
                                        errors[0].at(i).lWing = bigNum + buffer;
                                        verticalWing = true;
                                        // std::cout << "Wing adjusted" << std::endl;
                                    }
                                    if ((smallNum <= candidate.rWing) & (bigNum >= candidate.rWing)) {
                                        errors[0].at(i).rWing = smallNum - buffer;
                                        verticalWing = true;
                                        // std::cout << "Wing adjusted" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                horizontalIdx = horizontal[candidate.netNum].size() + 1;
                for (int j = 0; j < horizontal[candidate.netNum].size(); j++) {
                    if (i % 2 == 0) {
                        if ((candidate.head == horizontal[candidate.netNum][j].at(1)) & (candidate.tail == horizontal[candidate.netNum][j].at(2)) & (candidate.lWing == horizontal[candidate.netNum][j].at(0))) {
                            horizontalIdx = j;
                            horizontalNetNum = candidate.netNum;
                            // std::cout << "found left at " << horizontalIdx << std::endl;
                            // std::cout << horizontal[candidate.netNum][j].at(0) << std::endl;
                            // std::cout << horizontal[candidate.netNum][j].at(1) << std::endl;
                            // std::cout << horizontal[candidate.netNum][j].at(2) << std::endl;
                        }
                    } else {
                        if ((candidate.head == horizontal[candidate.netNum][j].at(2)) & (candidate.tail == horizontal[candidate.netNum][j].at(1)) & (candidate.lWing == horizontal[candidate.netNum][j].at(0))) {
                            horizontalIdx = j;
                            horizontalNetNum = candidate.netNum;
                            // std::cout << "found right at " << horizontalIdx << std::endl;
                            // std::cout << horizontal[candidate.netNum][j].at(0) << std::endl;
                            // std::cout << horizontal[candidate.netNum][j].at(1) << std::endl;
                            // std::cout << horizontal[candidate.netNum][j].at(2) << std::endl;
                        }
                    }
                }
                // std::cout << horizontalIdx << std::endl;
                if (horizontalIdx < horizontal[candidate.netNum].size()) {
                    errors[0].at(i).head = candidate.head + (((i % 2) * 2 - 1) * buffer);
                    errors[0].at(i).lWing = candidate.lWing + (((i % 2) * 2 - 1) * buffer);
                    errors[0].at(i).rWing = candidate.rWing - (((i % 2) * 2 - 1) * buffer);
                    // std::cout << errors[0].at(i).head << " " << errors[0].at(i).lWing << " " << errors[0].at(i).rWing  << std::endl;
                    bool horizontalHead = true;
                    bool horizontalWing = true;
                    while (!(errors[0].at(i) == candidate) | horizontalHead | horizontalWing) {
                        horizontalHead = false;
                        horizontalWing = false;
                        candidate = errors[0].at(i);
                        for (int m = 0; m < horizontal[candidate.netNum].size(); m++) {
                            int smallNum = horizontal[candidate.netNum][m][1];
                            int bigNum = horizontal[candidate.netNum][m][2];
                            if ((smallNum <= candidate.head) & (bigNum >= candidate.head)) {
                                if (i % 2 == 0) {
                                    if ((horizontal[candidate.netNum][m][0] >= candidate.lWing) && (horizontal[candidate.netNum][m][0] <= candidate.rWing)) {
                                        errors[0].at(i).head = smallNum - buffer;
                                        horizontalHead = true;
                                    }
                                } else {
                                    if ((horizontal[candidate.netNum][m][0] >= candidate.rWing) && (horizontal[candidate.netNum][m][0] <= candidate.lWing)) {
                                        errors[0].at(i).head = bigNum + buffer;
                                        horizontalHead = true;
                                    }
                                }
                            }
                        }
                        candidate = errors[0].at(i);
                        for (int n = 0; n < vertical[candidate.netNum].size(); n++) {
                            int smallNum = vertical[candidate.netNum][n][1];
                            int bigNum = vertical[candidate.netNum][n][2];
                            if (i % 2 == 0) {
                                if ((vertical[candidate.netNum][n][0] >= candidate.head) & (vertical[candidate.netNum][n][0] <= candidate.issuePoint)) {
                                    if ((smallNum <= candidate.lWing) & (bigNum >= candidate.lWing)) {
                                        errors[0].at(i).lWing = smallNum - buffer;
                                        horizontalWing = true;
                                        // std::cout << "Wing adjusted" << std::endl;
                                    }
                                    if ((smallNum <= candidate.rWing) & (bigNum >= candidate.rWing)) {
                                        errors[0].at(i).rWing = bigNum + buffer;
                                        horizontalWing = true;
                                        // std::cout << "Wing adjusted" << std::endl;
                                    }
                                }
                            } else {
                                if ((vertical[candidate.netNum][n][0] <= candidate.head) & (vertical[candidate.netNum][n][0] >= candidate.issuePoint)) {
                                    if ((smallNum <= candidate.lWing) & (bigNum >= candidate.lWing)) {
                                        errors[0].at(i).lWing = bigNum + buffer;
                                        horizontalWing = true;
                                    }
                                    if ((smallNum <= candidate.rWing) & (bigNum >= candidate.rWing)) {
                                        errors[0].at(i).rWing = smallNum - buffer;
                                        horizontalWing = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (i == 0) {
                distance = errors[0].at(0);
                minIdx = i;
            } else if ((candidate < distance) & (candidate.head <= 100) & (candidate.head >= 0) & (candidate.lWing <= 100) & (candidate.lWing >= 0) & (candidate.rWing <= 100) & (candidate.rWing >= 0)) {
                distance = candidate;
                minIdx = i;
                // std::cout << minIdx << std::endl;
                // std::cout << "issuePoint: " << distance.issuePoint << std::endl;
                // std::cout << "otherPoint: " << distance.rWing << std::endl;
            }
        }
        if ((verticalIdx < vertical[verticalNetNum].size()) & (horizontalIdx < horizontal[horizontalNetNum].size())) {
            if (minIdx < 2) {
                int hLeft = distance.issuePoint;
                int hRight = distance.head;

                std::vector<int> tempH;
                if (hRight < hLeft) {
                    hRight = hLeft;
                    hLeft = distance.head;
                }

                tempH.push_back(distance.lWing);
                tempH.push_back(hLeft);
                tempH.push_back(hRight);
                horizontal[distance.movingNet].push_back(tempH);
                tempH.clear();
                tempH.push_back(distance.rWing);
                tempH.push_back(hLeft);
                tempH.push_back(hRight);
                horizontal[distance.movingNet].push_back(tempH);
                std::cout << "NetNum: " << distance.netNum << std::endl;
                std::cout << "isV: " << distance.isV << std::endl;
                std::cout << "movingNet: " << distance.movingNet << std::endl;
                std::cout << "issuePoint: " << distance.issuePoint << std::endl;
                std::cout << "head: " << distance.head << std::endl;
                std::cout << "tail: " << distance.tail << std::endl;
                std::cout << "lWing: " << distance.lWing << std::endl;
                std::cout << "rWing: " << distance.rWing << std::endl << std::endl;

                int hTop = distance.lWing;
                int hBottom = distance.rWing;

                if (hTop < hBottom) {
                    hTop = hBottom;
                    hBottom = distance.lWing;
                }
                std::vector<int> tempV;
                tempV.push_back(distance.head);
                tempV.push_back(hBottom);
                tempV.push_back(hTop);
                vertical[distance.movingNet].push_back(tempV);

                int verticalTop = vertical[distance.movingNet][verticalIdx].at(2);
                vertical[distance.movingNet][verticalIdx].at(2) = hBottom;
                if (vertical[distance.movingNet][verticalIdx].at(1) > vertical[distance.movingNet][verticalIdx].at(2)) {
                    vertical[distance.movingNet][verticalIdx].at(2) = vertical[distance.movingNet][verticalIdx].at(1);
                    vertical[distance.movingNet][verticalIdx].at(1) = hBottom;
                }
                tempV.clear();
                tempV.push_back(vertical[distance.movingNet][verticalIdx].at(0));
                tempV.push_back(hTop);
                tempV.push_back(verticalTop);
                if (hTop > verticalTop) {
                    tempV.at(1) = tempV.at(2);
                    tempV.at(2) = hTop;
                }
                vertical[distance.movingNet].push_back(tempV);

            } else {
                int vTop = distance.issuePoint;
                int vBottom = distance.head;

                std::vector<int> tempV;
                if (vTop < vBottom) {
                    vTop = vBottom;
                    vBottom = distance.issuePoint;
                }

                tempV.push_back(distance.lWing);
                tempV.push_back(vBottom);
                tempV.push_back(vTop);
                vertical[distance.movingNet].push_back(tempV);
                tempV.clear();
                tempV.push_back(distance.rWing);
                tempV.push_back(vBottom);
                tempV.push_back(vTop);
                vertical[distance.movingNet].push_back(tempV);


                int hLeft = distance.lWing;
                int hRight = distance.rWing;

                if (hRight < hLeft) {
                    hRight = hLeft;
                    hLeft = distance.rWing;
                }
                std::vector<int> tempH;
                tempH.push_back(distance.head);
                tempH.push_back(hLeft);
                tempH.push_back(hRight);
                horizontal[distance.movingNet].push_back(tempH);

                int horizontalRight = horizontal[distance.movingNet][horizontalIdx].at(2);
                horizontal[distance.movingNet][horizontalIdx].at(2) = hLeft;
                if (horizontal[distance.movingNet][horizontalIdx].at(1) > horizontal[distance.movingNet][horizontalIdx].at(2)) {
                    horizontal[distance.movingNet][horizontalIdx].at(2) = horizontal[distance.movingNet][horizontalIdx].at(1);
                    horizontal[distance.movingNet][horizontalIdx].at(1) = hLeft;
                }
                tempH.clear();
                tempH.push_back(horizontal[distance.movingNet][horizontalIdx].at(0));
                tempH.push_back(hRight);
                tempH.push_back(horizontalRight);
                horizontal[distance.movingNet].push_back(tempH);
                if (hRight > horizontalRight) {
                    tempH.at(1) = tempH.at(2);
                    tempH.at(2) = hRight;
                }
                // std::cout << "head: " << distance.head << std::endl;
                // std::cout << "hLeft: " << hLeft << std::endl;
                // std::cout << "hRight: " << hRight << std::endl << std::endl;
            }
        }

        errors.erase(errors.begin());
    }
}