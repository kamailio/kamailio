# TLS reload issue

## **Without CA List**

| enable_shared_ctx | At start | After ~1000 calls to tls.reload |
|--------|--------|-------|
| 0      | total: 14680064<br> free: 9612064<br> used: 3342048<br> real_used: 5068000<br> max_used: 5068400<br> fragments: 3 | total: 14680064<br> free: 7689136<br> used: 4285648<br> real_used: 6990928<br> max_used: 6994592<br> fragments: 11 |
| 1      | total: 14680064<br> free: 11115744<br> used: 2769760<br> real_used: 3564320<br> max_used: 3565120<br> fragments: 3 | total: 14680064<br> free: 11026000<br> used: 2811904<br> real_used: 3654064<br> max_used: 3688576<br> fragments: 8 |

## **With CA List**

| enable_shared_ctx | At start | After ~1000 calls to tls.reload |
|--------|--------|-------|
| 0      | total: 14680064<br> free: 9241712<br> used: 3490528<br> real_used: 5438352<br> max_used: 5438752<br> fragments: 3 | total: 14680064<br> free: 6883520<br> used: 4647632<br> real_used: 7796544<br> max_used: 7808000<br> fragments: 10 |
| 1      | total: 14680064<br> free: 11096976<br> used: 2777776<br> real_used: 3583088<br> max_used: 3583888<br> fragments: 3 | total: 14680064<br> free: 10990512<br> used: 2826112<br> real_used: 3689552<br> max_used: 3727440<br> fragments: 6 |
