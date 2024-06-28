import json
import logging
import boto3
import os
import datetime

# Set up logging
logger = logging.getLogger()
logger.setLevel(logging.INFO)

# DynamoDB
dynamodb = boto3.resource('dynamodb')
table_name = os.environ['TABLE_NAME']
table = dynamodb.Table(table_name)

def lambda_handler(event, context):
    # Formulate UUID of request from current UTC
    uuid = datetime.datetime.utcnow().isoformat()
    
    # Fetch current UTC time as String
    time = datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
    
    try:
        # Parse JSON data from POST request body
        body = json.loads(event['body'])
        
        # Log the parsed body
        logger.info('Parsed body: %s', json.dumps(body))
        
        # Pull out attributes from JSON body
        HOTS_ID = body.get('id')
        tfi = body.get('tfi')
        temperature = body.get('temperature')
        humidity = body.get('humidity')
        alert = body.get('alert')
        
        # Check if any attribute is None
        if id is None or temperature is None or humidity is None:
            raise ValueError("Missing required attributes in JSON body")
 
        
        # Create DynamoDB entry
        entry = {
            'UUID': uuid,
            'HOTS_ID': HOTS_ID,
            'tfi': tfi,
            'temperature': temperature,
            'humidity': humidity,
            'alert': alert,
            'time': time
        }
        
        # Log the entry to be inserted
        logger.info('Inserting entry: %s', json.dumps(entry))
        
        response = table.put_item(Item=entry)

        # Log the received POST data
        logger.info('Received data: %s', json.dumps(body))

        # Echo the data back
        response = {
            'statusCode': 200,
            'body': json.dumps({
                'message':'Data received!',
                'received_data': body,
                'alert': alert,
                'UUID': uuid,
                'time': time
            })
        }
    except Exception as e:
        # Log the exception
        logger.error('Error processing data: %s', str(e))
        response = {
            'statusCode': 500,
            'body': json.dumps({'message': 'Error receiving data!'}),
            'UUID': uuid,
            'alert': alert,
            'time': time
        }

    return response