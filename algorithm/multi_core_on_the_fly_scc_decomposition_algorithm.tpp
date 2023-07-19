template<typename GraphNode>
GraphNode*
internal_::get_vp_from(GraphNode* vp)
{
    GraphNode* representative = vp;
    GraphNode* candidate;

    do
    {
        representative = representative->find_set();
        candidate = representative->get_node_from_set();
    } while (!candidate && representative->find_set() != representative);

    return candidate;
}

template<typename GraphNode>
void
multi_core_on_the_fly_scc_decomposition_algorithm(GraphNode* start_node, const uint64_t thread_id)
{
    using GNiterator = typename GraphNode::iterator;

    std::stack<GraphNode*>                        stack_path_nodes;
    std::stack<GraphNode*>                        stack_explore_nodes_v; // nodes v
    std::stack<GraphNode*>                        stack_explore_nodes_vp; // nodes v'
    std::stack<std::pair<GNiterator, GNiterator>> stack_explore_iterators; // iterators to neighbors of stack_explore_nodes_vp

    // dfs init
    start_node->add_mask(thread_id);
    stack_path_nodes.emplace(start_node);
    stack_explore_nodes_v.emplace(start_node);

    // iterative dfs
    while (!stack_explore_nodes_v.empty())
    {
        // pick random v' and insert its random iterators
        if (stack_explore_nodes_v.size() == stack_explore_nodes_vp.size() + 1)
        {
            // get candidate node which is not done to prevent creating unnecessary iterators, which can be expensive
            GraphNode* candidate = internal_::get_vp_from(stack_explore_nodes_v.top());
            while (candidate && candidate->is_done())
                candidate = internal_::get_vp_from(stack_explore_nodes_v.top());

            if (!candidate)
            {
                if (stack_explore_nodes_v.top()->mark_as_dead())
                {
                    GraphNode *v=stack_explore_nodes_v.top();
                    // You can report SCC here after locking and if you would remember more data
//                    std::cout<<v->find_set()->get_label();
                }

                if (stack_explore_nodes_v.top() == stack_path_nodes.top())
                    stack_path_nodes.pop();

                stack_explore_nodes_v.pop();
            }
            else
            {
                stack_explore_nodes_vp.emplace(candidate);
                stack_explore_iterators.emplace(candidate->get_random_neighbors_iterators());
            }
        }
        // we have everything set, just continue in iterating iterators
        else
        {
            assert(stack_explore_nodes_v.size() == stack_explore_nodes_vp.size());

            // done all neighbors of stack_explore_nodes_vp.top()
            if (stack_explore_iterators.top().first == stack_explore_iterators.top().second)
            {
                stack_explore_nodes_vp.top()->mark_as_done();
                stack_explore_nodes_vp.pop();
                stack_explore_iterators.pop();
            }
            // active node v' was marked dead, no need to search him anymore
            else if (stack_explore_nodes_vp.top()->is_done())
            {
                stack_explore_nodes_vp.pop();
                stack_explore_iterators.pop();
            }
            // take w from iterators and decide what next
            else
            {
                GraphNode* w = *stack_explore_iterators.top().first;
                ++stack_explore_iterators.top().first;

                // w is dead, do nothing
                if (w->is_dead())
                {
                }
                // "recursively" call on w
                else if (!w->has_mask(thread_id))
                {
                    w->add_mask(thread_id);
                    stack_path_nodes.emplace(w);
                    stack_explore_nodes_v.emplace(w);
                }
                // found cycle, merge until w and stack_explore_nodes_v.top() are in same set
                else
                {
                    while (!stack_explore_nodes_v.top()->same_set(w))
                    {
                        assert(stack_explore_nodes_v.size() >= 2);

                        GraphNode* tmp = stack_path_nodes.top();
                        stack_path_nodes.pop();

                        // union set can fail
                        while (!tmp->union_set(stack_path_nodes.top()));
                    }
                }
            }
        }
    }
}
