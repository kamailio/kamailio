# Python Mocking Framework for testing #

Generate a mocking framework base on the output of app_python.api_list

### Usage:
```
/usr/sbin/kamctl rpc app_python.api_list > api.json
./kemi_mock.py api.json > KSR.py
```

*Note:* Python 3.2 doesn't support the Union type. To generate KSR.py without
the Union type add the --no-union flag

```
./kemi_mock.py api.json --no-union > KSR.py
```

### Usage with Documentation:
run the generate.py from the following project

https://github.com/braams/vscode-kamailio-hover

this creates a modules.json file in a tmp directory. This file can be used to generate the KSR.py file with documentation

pass the modules.json as a 2nd parameter to kemi_mock.py
```
./kemi_mock.py api.json modules.json > KSR.py
```

## Mocking 
Return values can be injected through the dictionary \_mock\_data

```python

#set return value for all calls to the function
_mock_data[module][function] = value

#set return value for specific parameters being passed
_mock_data[module][function][param_value] = value

#call the function myFunc when func is passed, return of myFunc will
#be the value module.function returns
_mock_data[module][function] = myFunc
```

see test.py for example usage


