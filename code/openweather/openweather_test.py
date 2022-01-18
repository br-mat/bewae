# importing requests and json
import requests, json
# base URL
BASE_URL = "https://api.openweathermap.org/data/2.5/onecall?lat=47.07&lon=15.42&appid=b05760615139c231301ec828d011e7b1"
# City Name CITY = "Hyderabad"
# API key API_KEY = "Your API Key"
# upadting the URL
URL = BASE_URL #+ "q=" + CITY + "&appid=" + API_KEY
# HTTP request
response = requests.get(URL)
# checking the status code of the request
if response.status_code == 200:
   # getting data in the json format
   data = response.json()
   # getting the main dict block
   #main = data['main']
   # getting temperature
   #temperature = main['temp']
   # getting the humidity
   #humidity = main['humidity']
   # getting the pressure
   #pressure = main['pressure']
   # weather report
   #report = data['weather']
   #print(f"{CITY:-^30}")
   #print(f"Temperature: {temperature}")
   #print(f"Humidity: {humidity}")
   #print(f"Pressure: {pressure}")
   #print(f"Weather Report: {report[0]['description']}")
   print(data)
else:
   # showing the error message
   print("Error in the HTTP request")
