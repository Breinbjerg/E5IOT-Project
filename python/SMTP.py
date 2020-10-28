import smtplib
import ssl

"""
Using python to see communication between 
smtp server and client. 
This is only to test and debug, to make it easier to implement on 
Particle argon. 
"""


smtp_server = "smtp.mailgun.org"
port = 587  # For starttls
sender_email = "Insert here"
receiver_email = "Insert here"
password = "Insert here"

message = """\
Subject: Hi there

This message is sent from Python."""

# Create a secure SSL context
context = ssl.create_default_context()

# Try to log in to server and send email
try:
    server = smtplib.SMTP(smtp_server, port)
    server.set_debuglevel(2)
    server.ehlo()  # Can be omitted
    server.starttls(context=context)  # Secure the connection
    server.ehlo()  # Can be omitted
    server.login(sender_email, password)
    server.sendmail(sender_email, receiver_email, message)
except Exception as e:
    # Print any error messages to stdout
    print(e)
finally:
    server.quit()
