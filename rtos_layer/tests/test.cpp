#include <rtos_layer/queue.hpp>
#include <rtos_layer/mutex.hpp>
#include <rtos_layer/notification.hpp>
#include <cstdlib>
// Checks remain active in Release builds.
#define CHECK(x) do { if(!(x)) std::abort(); } while(false)
int main() {
    using namespace rtos_layer;
    static_assert(!std::is_copy_constructible_v<StaticQueue<int,2>>);
    static_assert(!std::is_move_constructible_v<StaticMutex>);
    StaticQueue<int,2> q;
    int out=99;
    CHECK(!q.receive(out,7) && out==99 && last_timeout==7);
    CHECK(q.send(10,3) && last_timeout==3);
    CHECK(q.send(20)); CHECK(!q.send(30));
    CHECK(q.receive(out) && out==10);
    CHECK(q.send(30)); // circular wrap
    CHECK(q.receive(out) && out==20);
    BaseType_t woken=pdFALSE;
    CHECK(q.receive_from_isr(out,woken) && out==30 && woken==pdTRUE);
    CHECK(!q.receive_from_isr(out,woken) && woken==pdTRUE);
    woken=pdFALSE;
    CHECK(q.send_from_isr(40,woken) && woken==pdTRUE);
    CHECK(q.receive(out) && out==40);
    StaticMutex mutex;
    {
        MutexGuard guard(mutex,5);
        CHECK(guard.owns_lock() && last_timeout==5);
        { MutexGuard failed(mutex,9); CHECK(!failed.owns_lock()); }
        CHECK(releases==0);
    }
    CHECK(releases==1);
    CHECK(mutex.lock(0)); CHECK(mutex.unlock());
    Notification notification(&current_task);
    notification.give(); woken=pdFALSE;
    notification.give_from_isr(woken); CHECK(woken==pdTRUE);
    CHECK(Notification::take(false,11)==2 && last_timeout==11);
    CHECK(Notification::take(true,0)==1);
    CHECK(Notification::take(true,0)==0);
}
