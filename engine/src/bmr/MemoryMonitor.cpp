#include "MemoryMonitor.h"
#include "BlazingMemoryResource.h"
#include "execution_graph/logic_controllers/PhysicalPlanGenerator.h"
#include "execution_graph/logic_controllers/taskflow/executor.h"

namespace ral {

    MemoryMonitor::MemoryMonitor(std::shared_ptr<ral::batch::tree_processor> tree,
                                 std::map<std::string, std::string> config_options)
    : finished(false), tree(tree), resource(&blazing_device_memory_resource::getInstance()) {

        period = std::chrono::milliseconds(50);
        auto it = config_options.find("MEMORY_MONITOR_PERIOD");
        if (it != config_options.end()) {
            period = std::chrono::milliseconds(std::stoull(config_options["MEMORY_MONITOR_PERIOD"]));
        }
    }

    bool MemoryMonitor::need_to_free_memory(){
        return resource->get_memory_used() > resource->get_memory_limit();
    }

    void MemoryMonitor::finalize(){
        std::unique_lock<std::mutex> lock(finished_lock);
        finished = true;
        lock.unlock();
        condition.notify_all();
        this->monitor_thread.join();
    }

    void MemoryMonitor::start(){

        this->monitor_thread = BlazingThread([this](){
            std::unique_lock<std::mutex> lock(finished_lock);
            while(!condition.wait_for(lock, period, [this] { return this->finished; })){
                if (need_to_free_memory()){
                    downgradeCaches(&tree->root);

                    std::shared_ptr<spdlog::logger> logger = spdlog::get("batch_logger");

                    std::vector<std::unique_ptr<ral::execution::task>> tasks;
                     // if after downgrading all caches there is still too much consumption, lets try to downgrade data in tasks
                     // Lets pull tasks from the back of the queue, since they are ones that will not be operated on immediatelly
                    while (need_to_free_memory()){

                        std::unique_ptr<ral::execution::task> task = ral::execution::executor::get_instance()->remove_task_from_back();
                        if (task != nullptr){
                            if (logger){
                                logger->info("{query_id}|||{info}|||||",
                                    "query_id"_a=tree->context->getContextToken(),
                                    "info"_a="MemoryMonitor about to free memory from tasks");
                            }
                            std::vector<std::unique_ptr<ral::cache::CacheData > > inputs = task->release_inputs();
                            for (std::size_t i = 0; i < inputs.size(); i++){
                                inputs[i] = std::move(inputs[i]->downgradeCacheData(std::move(inputs[i]), "", tree->context));
                            }
                            task->set_inputs(std::move(inputs));
                            tasks.push_back(std::move(task));
                        } else {
                            break;
                        }
                    }
                    // we have now decached the inputs from either enough tasks to get below the memory limit or there are no more tasks to work with
                    // Now lets add the tasks back to the queue in the same order they were in
                    if (tasks.size() > 0){
                        for (int i = tasks.size() - 1; i >= 0; i--){
                            ral::execution::executor::get_instance()->add_task(std::move(tasks[i]));
                        }
                    }
                    ral::execution::executor::get_instance()->notify_memory_safety_cv();
                    if (tasks.size() > 0){
                        if (logger){
                            logger->info("{query_id}|||{info}|||||",
                                "query_id"_a=tree->context->getContextToken(),
                                "info"_a="MemoryMonitor successfully freed memory from tasks");
                        }
                    }
                }
            }
        });
    }

    void MemoryMonitor::downgradeCaches(ral::batch::node* starting_node){
        if (starting_node->kernel_unit->get_id() != 0) { // we want to skip the output node
            for (auto iter = starting_node->kernel_unit->output_.cache_machines_.begin(); 
                    iter != starting_node->kernel_unit->output_.cache_machines_.end(); iter++) {
                size_t amount_downgraded = 0;
                do {
                    amount_downgraded = iter->second->downgradeCacheData();
                } while (amount_downgraded > 0 && need_to_free_memory()); // if amount_downgraded is 0 then there is was nothing left to downgrade
            }
        }
        if (need_to_free_memory()){
            if (starting_node->children.size() == 1){
                downgradeCaches(starting_node->children[0].get());
            } else if (starting_node->children.size() > 1){
                std::vector<BlazingThread> threads;
                for (auto node : starting_node->children){
                    threads.push_back(BlazingThread([this, node](){
                        this->downgradeCaches(node.get());
                    }));
                }
                for(auto & thread : threads) {
                    thread.join();
                }
            }
        }
    }
}  // namespace ral
