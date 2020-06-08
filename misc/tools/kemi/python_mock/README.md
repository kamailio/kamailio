# Python Mocking Framework for testing #

Generate a mocking framework base on the output of app_python.api_list

Usage:
```
/usr/sbin/kamctl rpc app_python.api_list > api.json
./kemi_mock.py api.json > KSR.py
```

Return values can be injected through the dictionary \_mock\_data

```python

#set retun value for all calls to the function
_mock_data[module][function] = value

#set retun value for specific parameters being passed
_mock_data[module][function][param_value] = value
```

see test.py for example usage


