NOTIFY sip:alice@atlanta.example.com SIP/2.0
Via: SIP/2.0/UDP x;branch=z9hG4bK3
From: <sip:b@b.org>;tag=1
To: <sip:a@a.org>;tag=2
Call-ID: n1
Event: message-summary
Subscription-State: active;expires=600
CSeq: 2 NOTIFY
Content-Type: application/simple-message-summary
Content-Length: 46

Messages-Waiting: yes
Voice-Message: 2/8 (1/2)
