#include <ir/ops/constant.h>
#include <ir/op_utils.h>
#include <ir/visitor.h>
#include <scheduler/scheduler.h>
#include <unordered_map>

using namespace nncase;
using namespace nncase::ir;
using namespace nncase::scheduler;

namespace
{
std::unordered_map<node_opcode, std::function<void(ir::node &, ir::output_connector &, allocation_context)>> g_allocators;
}

void nncase::scheduler::register_input_allocator(node_opcode opcode, std::function<void(ir::node &, ir::output_connector &, allocation_context)> allocator)
{
    g_allocators.emplace(opcode, std::move(allocator));
}

class schedule_visitor : public dfs_ir_visitor
{
public:
    using dfs_ir_visitor::visit;

    schedule_visitor(allocation_context &context, std::vector<node *> &compute_sequence)
        : context_(context), compute_sequence_(compute_sequence)
    {
    }

    bool visit(node &node) override
    {
        for (auto &&out : node.outputs())
        {
            for (auto &&in : out.connections())
            {
                auto &in_node = in->owner();
                auto it = g_allocators.find(in_node.opcode());
                if (it != std::end(g_allocators))
                {
                    it->second(in_node, out, context_);
                }
                else
                {
                    context_.allocate_default(out);
                }
            }
        }

        // check overlap
        {
            std::vector<memory_allocation> inputs, outputs;
            for (auto &&out : node.outputs())
                outputs.emplace_back(context_.allocations().at(&out));

            for (auto &&in : node.inputs())
                inputs.emplace_back(context_.allocations().at(in.connection()));

            for (auto &&m : inputs)
            {
                assert(std::none_of(outputs.begin(), outputs.end(), [&](const memory_allocation rhs) {
                    return rhs.overlap(m);
                }));
            }
        }

        compute_sequence_.emplace_back(&node);

        // Pin output
        if (node.opcode() != op_output)
        {
            for (auto &&in : node.inputs())
            {
                auto out = in.connection();
                assert(out);

                // Pin constant and input
                if (out->type() != mem_const && out->owner().opcode() != op_input)
                {
                    context_.release(*out);
                }
            }
        }

        return false;
    }

private:
    allocation_context &context_;
    std::vector<node *> &compute_sequence_;
};

allocation_context::allocation_context(const std::unordered_map<memory_type_t, memory_allocator *> &allocators)
    : allocators_(allocators)
{
}

void allocation_context::allocate_default(ir::output_connector &conn)
{
    auto allocator = allocators_.find(conn.memory_type());
    if (allocator == allocators_.end())
        throw std::runtime_error("Allocator is not found");

    auto it = memory_map_.find(&conn);
    if (it == memory_map_.end())
    {
        auto size = allocator->second->get_bytes(conn.type(), conn.shape());
        auto &node = allocator->second->allocate(size);
        memory_map_.emplace(&conn, &node);
        allocations_.emplace(&conn, memory_allocation { conn.memory_type(), node.safe_start(), size });
    }
    else
    {
        it->second->add_ref();
    }
}

void allocation_context::release(ir::output_connector &conn)
{
    auto node = memory_map_.find(&conn);
    if (node != memory_map_.end())
        node->second->release();
}

void nncase::scheduler::schedule(xtl::span<output_node *> outputs, allocation_context &context, std::vector<ir::node *> &compute_sequence)
{
    schedule_visitor visitor(context, compute_sequence);
    visitor.visit(outputs);
}