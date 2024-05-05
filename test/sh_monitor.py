#!/usr/bin/python3

import sys, traceback
import socket
import logging
import getpass
import struct
import time

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5 import uic
from PyQt5 import QtSerialPort
#from PyQt5 import QSettings
#from PyQt5.QtCore import QTimer

COMM_BESC  =0x5a  # frame start
COMM_BSHIFT=0xa5  # "shifted byte" marker
COMM_SESC  =0x55  # "shifted" COMM_BESC  
COMM_SSHIFT=0xaa  # "shifted" COMM_BSHIFT  (COMM_BESC xor COMM_BSHIFT must result 0xff)
TICKS_PER_SEC = 7200
# frame types (commands)
COMM_CMD_IGNORE     = 0   # ignore, do not send or do anything
COMM_CMD_GETINFO    = 1   # please, send the own time (32bits) and signature (TBD)
COMM_CMD_SETTIME    = 2   # set time, time in the form of 48 bits number (LSB first): 2B with tick_cnt (ms/1000*3600),
                          # sec - 6b, min - 6b, hour - 5b, day - 5b, month - 4b,
                          # year - 6b (counted from 1.01.2020) - send as a broadcast, no answer needed
COMM_CMD_SETPAR     = 3   # set parameter(s) (addr, values...)                     
COMM_CMD_OWSEARCH   = 4   # search one_wire bus
COMM_CMD_OW_TEMP    = 5   # measure temperature


frm_no=0
frm_conf=0

rcv_frm_no=5
rcv_frm_conf=10

src=0
#dest=1


def DEBUGprintf1(a):
  print(a)

def DEBUGprintf2(a, b):
  print(a, b)

class PortSettings(QtWidgets.QDialog):

  def __init__(self, *args, **kwargs):
#    super().__init__(*args, **kwargs)
    super(PortSettings, self).__init__()
    uic.loadUi("optionsdialog.ui", self)
    self.device = self.findChild(QtWidgets.QLineEdit, 'device')
    self.speed_cb = self.findChild(QtWidgets.QComboBox, 'speed_cb')


class SH_Monitor(QtWidgets.QMainWindow):
  IDLE=0
  FIRMLOAD=1
  RUNPROG=2
#  TRANSM=3
  TEST=0
#  COMM_BESC=0xaa
#  COMM_BSHIFT=0x55
  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
#    super(SH_Monitor, self).__init__()
#    self.resize(1000, 1066)
    self.fsm_state=self.IDLE
    uic.loadUi("sh_monitor.ui", self)

    self.mainlayout = self.findChild(QtWidgets.QVBoxLayout, 'main_layout') # Find the main layount

#    self.layout().setContentsMargins(20, 20, 20, 20)
    self.statusbar = QtWidgets.QStatusBar(self)
    self.statusbar.addPermanentWidget(QtWidgets.QLabel("V 0.2, M.Grecki, ElMaG, 2020", self))
    self.mainlayout.addWidget(self.statusbar)
    self.port_status=QtWidgets.QLabel("")
    self.statusbar.addWidget(self.port_status)

    self.log_cb = self.findChild(QtWidgets.QCheckBox, 'log_cb') # Find the check box

#    self.onoff_cb = self.findChild(QtWidgets.QCheckBox, 'onoff_cb') # Find the check box
#    self.onoff_cb.clicked.connect(self.onoff)
    
    self.loghex_cb = self.findChild(QtWidgets.QCheckBox, 'loghex_cb') # Find the check box

    bt = self.findChild(QtWidgets.QPushButton, 'test_bt') # Find the ui control
    bt.clicked.connect(self.test)

#    self.fu_bt = self.findChild(QtWidgets.QPushButton, 'firmware_upload_bt') # Find the ui control
#    self.fu_bt.clicked.connect(self.firmware_upload)

    self.dev_sel_sb = self.findChild(QtWidgets.QSpinBox, 'dev_sel_sb') # Find the ui control
#    self.fu_bt.clicked.connect(self.firmware_upload)

    self.dev_get_info_bt = self.findChild(QtWidgets.QPushButton, 'dev_get_info_bt') # Find the ui control
    self.dev_get_info_bt.clicked.connect(self.dev_get_status)

    self.dev_set_time_bt = self.findChild(QtWidgets.QPushButton, 'dev_set_time_bt') # Find the ui control
    self.dev_set_time_bt.clicked.connect(self.dev_set_time)

    self.dev_time_now_bt = self.findChild(QtWidgets.QPushButton, 'dev_time_now_bt') # Find the ui control
    self.dev_time_now_bt.clicked.connect(self.dev_time_now)

    self.dev_par_addr_sb = self.findChild(QtWidgets.QSpinBox, 'dev_par_addr_sb') # Find the ui control

    self.dev_par_vals_le = self.findChild(QtWidgets.QLineEdit, 'dev_par_vals_le') # Find the ui control

    self.dev_par_set_bt = self.findChild(QtWidgets.QPushButton, 'dev_par_set_bt') # Find the ui control
    self.dev_par_set_bt.clicked.connect(self.dev_par_set)

    self.dev_ow_search_bt = self.findChild(QtWidgets.QPushButton, 'dev_ow_search_bt') # Find the ui control
    self.dev_ow_search_bt.clicked.connect(self.dev_ow_search)
    self.dev_ow_addrs_cb = self.findChild(QtWidgets.QComboBox, 'dev_ow_addrs_cb') # Find the ui control

    self.dev_ow_temperature_bt = self.findChild(QtWidgets.QPushButton, 'dev_ow_temperature_bt') # Find the ui control
    self.dev_ow_temperature_bt.clicked.connect(self.dev_ow_temperature)

    bt = self.findChild(QtWidgets.QPushButton, 'port_settings_bt') # Find the ui control
    bt.clicked.connect(self.port_settings)

#    self.sign_ll = self.findChild(QtWidgets.QLabel, 'sign_ll') # Find the label

    self.log_text = self.findChild(QtWidgets.QTextEdit, 'log_text')
    self.prog_bar = self.findChild(QtWidgets.QProgressBar, 'prog_bar')

    self.port=QtSerialPort.QSerialPort()
    self.port.readyRead.connect(self.port_readyread)
    self.port.setFlowControl(QtSerialPort.QSerialPort.NoFlowControl)
    settings=QtCore.QSettings("ElMaG", "SH_Monitor")
    settings.beginGroup("ComPort")
    self.port.setPortName(settings.value("device", "/dev/ttyUSB0"))
#    self.port.setBaudRate(int(settings.value("speed", 57600)))
    self.port.setBaudRate(int(settings.value("speed", 115200)))
    settings.endGroup()
    self.port.open(QtCore.QIODevice.ReadWrite);
    self.put_log("Port: "+self.port.portName()+", "+str(self.port.baudRate()))

    self.timer = QtCore.QTimer()
    self.timer.timeout.connect(self.runbackground)

    if self.port.isOpen():
      self.port.setDataTerminalReady(False)
#      self.fu_bt.setEnabled(True)
      self.put_log("ComPort opened...")
      self.port_status.setText("Port: "+self.port.portName()+", "+str(self.port.baudRate()))
#      self.timer.start(1000)
    else:
#      self.fu_bt.setEnabled(False)
      self.put_log("ComPort open failed...")
      self.port_status.setText("Port closed due to error")

    self.frm_no=0
    self.frm_conf=0
    self.frame_serial=0
    self.wasshift=0
    self.rcv_frm_src=0
    self.rcv_frm_dst=0
    self.rcv_frm_no=0
    self.rcv_frm_conf=0
    self.rcv_frm_ptr=0
    self.rcv_frm_crc=0




  def put_log(self, s):
    logging.info(s)
    self.log_text.moveCursor(QtGui.QTextCursor.End)
    self.log_text.insertPlainText(s)
    self.log_text.append("")


  def runbackground(self):
#    self.put_log("runbackground")
    self.dev_get_status()
    pass


  def port_readyread(self):
    self.log_text.moveCursor(QtGui.QTextCursor.End);
    while self.port.bytesAvailable():
      ch=self.port.read(1)[0]
      if self.log_cb.isChecked():
        if self.loghex_cb.isChecked():
#          self.log_text.insertPlainText("\\x{:02x}".format(ch))
          self.log_text.insertPlainText("{:02x} ".format(ch))
        else:
          if (ch>=32 and ch<=127 and ch!=ord('\\')) or ch==ord('\r'):
            self.log_text.insertPlainText(chr(ch))
          elif ch==ord('\\'):
            self.log_text.insertPlainText("\\")
          elif ch!=ord('\n'):
#            self.log_text.insertPlainText("\\x{:02x}".format(ch))
            self.log_text.insertPlainText("{:02x} ".format(ch))
          else:
            self.log_text.insertPlainText("ǁ")

      if self.fsm_state==self.FIRMLOAD:
        if ch==ord('#'):
          self.port.write(str.encode(self.firmware_buf[self.fptr]))
          self.fptr+=1
          self.prog_bar.setValue(100.0*self.fptr/len(self.firmware_buf))
          if self.fptr==len(self.firmware_buf):
            self.firmware_buf=""
            self.fsm_state=self.RUNPROG
            self.prog_bar.setValue(100);
      elif self.fsm_state==self.RUNPROG:
        if ch==ord('#'):
          self.port.write(b"g8000\r")
          self.fsm_state=self.IDLE;
      elif self.fsm_state==self.IDLE:
        if ch==COMM_BESC:
# start of new frame
          DEBUGprintf1("\nNew frame: ");
          self.frame_serial = not self.frame_serial
          self.wasshift=0
          self.rcv_frm_ptr=0
          self.rcv_frm_crc=0
        elif ch==COMM_BSHIFT:
          if self.wasshift:
            DEBUGprintf1("\ndouble SHIFT byte")
            self.frame_serial=0
          if self.frame_serial:
            self.wasshift=1
          else:
            DEBUGprintf1("\nSHIFT received outside the frame")
        else:
          if self.wasshift:
            self.wasshift=0;
            if ch==COMM_SESC:
              ch=COMM_BESC
            elif ch==COMM_SSHIFT:
              ch=COMM_BSHIFT
            else:
              DEBUGprintf1("\nSHIFT with wrong parameter received");
              self.frame_serial=0;
          if self.frame_serial:
#            print("self.rcv_frm_ptr:", self.rcv_frm_ptr)
            self.rcv_frm_crc = self.crc16_ccitt(self.rcv_frm_crc, (ch,))
            if self.rcv_frm_ptr==0: # frm_no
              self.rcv_frm_dst=ch
            elif self.rcv_frm_ptr==1: # frm_conf
              self.rcv_frm_src=ch
            elif self.rcv_frm_ptr==2: # frm_type
              self.rcv_frm_no=ch&0x0f
              self.rcv_frm_conf=(ch>>4)&0x0f
            elif self.rcv_frm_ptr==3: # frm_length_l
              self.rcv_frm_type=ch
            elif self.rcv_frm_ptr==4: # frm_length_h
              self.rcv_frm_length=ch
              self.rbuf=[0]*(self.rcv_frm_length)
            else:
              if self.rcv_frm_ptr-5 < self.rcv_frm_length:
                self.rbuf[self.rcv_frm_ptr-5]=ch
              if self.rcv_frm_ptr >= self.rcv_frm_length+6: # new frame just arrived
                print("new frame just arrived ("+str(self.rcv_frm_ptr)+"):", self.rcv_frm_no, self.rcv_frm_conf, self.rcv_frm_type, self.rcv_frm_length, end='')
                for i in self.rbuf:
                  print(" {:02x}".format(i), end='')
                print(" {:04x}".format(self.rcv_frm_crc))
                self.frame_serial=0 # ????
                rbuf=list(self.rbuf)
                print(rbuf)
                if self.rcv_frm_type==COMM_CMD_GETINFO: # time & signature
#                  self.sign_ll.setText("".join([chr(x) for x in self.rbuf[:-2]]))
                  ms=rbuf[0]+(rbuf[1]<<8)
                  lt=rbuf[2]+(rbuf[3]<<8)+(rbuf[4]<<16)+(rbuf[5]<<24)
                  sg=rbuf[6]+(rbuf[7]<<8)
                  print(hex(ms), hex(lt), sg)
                  t=QtCore.QTime((lt>>12)&0x1f, (lt>>6)&0x3f, lt&0x3f, int(1000*ms/TICKS_PER_SEC))
                  d=QtCore.QDate((lt>>26)+2020, (lt>>22)&0x0f, (lt>>17)&0x1f)
                  dt=QtCore.QDateTime(d,t)
                  print("time:", t, d, dt, QtCore.QDateTime.currentDateTime())
                  self.dev_dt.setDateTime(dt)
                  print(dt.toMSecsSinceEpoch(), QtCore.QDateTime.currentDateTime().currentMSecsSinceEpoch(), dt.toMSecsSinceEpoch()-QtCore.QDateTime.currentDateTime().currentMSecsSinceEpoch())
                elif self.rcv_frm_type==COMM_CMD_OWSEARCH: # 1-wire bus scan
                  s=""
                  for i in self.rbuf:
                    s+="{:02x}".format(i)
                  self.dev_ow_addrs_cb.addItem(s)
                elif self.rcv_frm_type==COMM_CMD_OW_TEMP: # 1-wire bus scan
                  print("{:02x}{:02x}".format(self.rbuf[1],self.rbuf[0]))
                  temp=(self.rbuf[1]<<8) | self.rbuf[0]
                  print(temp/16.0)
                elif self.rcv_frm_type==255: # error
                  if self.rbuf[0]==2: # CON_BAD_FRAME_NO : integer := 2;
                    self.frm_no=self.rcv_frm_conf+1
            self.rcv_frm_ptr+=1
          else:
            DEBUGprintf2("\ndata received outside the frame", "{:02x}".format(ch));


  def port_settings(self):
    """
    Port settings
    """
    print('Port settings')
    options=PortSettings()
    options.device.setText(self.port.portName())
#    print("Port:", self.port.baudRate(), options.speed_cb.findText(str(self.port.baudRate())))
    options.speed_cb.setCurrentIndex(max(options.speed_cb.findText(str(self.port.baudRate())),0))
    res=options.exec()
#    print(res)
    if res==QtWidgets.QDialog.Accepted:
#      print(options.device.text(), options.speed_cb.currentText())

      self.port.close()

      settings=QtCore.QSettings("ElMaG", "SH_Monitor")

      settings.beginGroup("ComPort")
      settings.setValue("device", options.device.text())
      settings.setValue("speed", options.speed_cb.currentText())
#      print("speed", settings.value("speed", 19200))
      self.port.setPortName(settings.value("device", "/dev/ttyUSB0"))
      self.port.setBaudRate(int(settings.value("speed", 19200)))
      settings.endGroup()

      self.port.open(QtCore.QIODevice.ReadWrite)
      self.put_log("Port: "+self.port.portName()+". "+str(self.port.baudRate()))
      if self.port.isOpen():
        self.port.setDataTerminalReady(False)
#        self.fu_bt.setEnabled(True)
        self.put_log("ComPort opened...")
        self.port_status.setText("Port: "+self.port.portName()+", "+str(self.port.baudRate()))
        self.timer.start(60000)
      else:
#        self.fu_bt.setEnabled(False)
        self.put_log("ComPort open failed...")
        self.port_status.setText("Port closed due to error")
        self.timer.stop()


  # def reset(self):
    # """
    # Reset
    # """
    # print('Reset')
# #    self.port.write(b"\xaaUa\r") # send ESC2 sequence
    # self.port.setDataTerminalReady(True)
    # time.sleep(5e-3) #QtCore.QObject.thread().msleep(5)
    # self.port.setDataTerminalReady(False)


  def dev_get_status(self):
    """
    Read device status
    """
    dev=self.dev_sel_sb.value()
#    print('dev_get_status')
#    self.port.write(b"?\r")
#    self.port.write(b"\xaa\x00\x00\x00\x04\x00\x01\x0b\x0c\x0d\x14\x24")
#    fr=self.frame(2, struct.pack("<B", self.TEST))
#    print("dev:", dev)
    fr=self.frame(dev, COMM_CMD_GETINFO, b"")
#    fr=self.frame(8)
#    print("frame:", fr.hex())
    self.port.write(fr)


  def dev_ow_search(self):
    """
    Set device time to current time
    """
    while(self.dev_ow_addrs_cb.count()):
      self.dev_ow_addrs_cb.removeItem(0)
    dev=self.dev_sel_sb.value()
#    d=QtCore.QDateTime.currentDateTime()
    fr=self.frame(dev, COMM_CMD_OWSEARCH, b"")
    print("frame:", fr.hex())
    self.port.write(fr)

  def dev_set_time(self):
    """
    Ask device to make 1-wire bus search
    """
    dev=self.dev_sel_sb.value()
#    d=QtCore.QDateTime.currentDateTime()
    d=self.dev_dt.dateTime()
    print("d", d)
    dt=d.time().second() | (d.time().minute()<<6) | (d.time().hour()<<12) | (d.date().day()<<17) | (d.date().month()<<22) | ((d.date().year()-2020)<<26)
    fr=self.frame(dev, COMM_CMD_SETTIME, struct.pack("<H", int(TICKS_PER_SEC*d.time().msec()/1000))+struct.pack("<L", dt))
    print("frame:", fr.hex())
    self.port.write(fr)

  def dev_ow_temperature(self):
    dev=self.dev_sel_sb.value()
    ad=self.dev_ow_addrs_cb.currentText()
    print(ad)
    if len(ad)==16 and ad[0]=="2" and ad[1]=="8":
      fr=self.frame(dev, COMM_CMD_OW_TEMP, bytes.fromhex(ad))
      print("frame:", fr.hex())
      self.port.write(fr)    

  def dev_time_now(self):
    d=self.dev_dt.setDateTime(QtCore.QDateTime.currentDateTime())
    self.dev_set_time()


  def dev_par_set(self):
    dev=self.dev_sel_sb.value()
#    d=QtCore.QDateTime.currentDateTime()
    d=self.dev_dt.dateTime()
    print("d", d)
    addr=self.dev_par_addr_sb.value()
    pars=self.dev_par_vals_le.text()
    p=pars.split()
    pl=struct.pack("<B", addr)
    for i in p:
      pl+=struct.pack("<B", int(i))
    fr=self.frame(dev, COMM_CMD_SETPAR, pl)
    print("frame:", fr.hex())
    self.port.write(fr)

  def run(self):
    """
    Run
    """
    print('Run')
    self.port.write(b"g8000\r") # send "goto" command


  # def firmware_upload(self):
    # """
    # Upload firmware
    # """
# #    print('Upload firmware')
# #    self.put_log(QtCore.QDir.currentPath()+"/../firmware");

    # fn=QtWidgets.QFileDialog.getOpenFileName(self, "Firmware file name", QtCore.QDir.currentPath()+ "/../firmware", "Hex files (*.hex *.ihx);;All files (*.*)")[0]

# #    print("filename:", fn)

    # if fn!="":
      # file=open(fn, "r")
      # self.firmware_buf=file.readlines()
# #      print("file:", self.firmware_buf)
      # for i in range(len(self.firmware_buf)):
        # self.firmware_buf[i]=self.firmware_buf[i].replace('\n', '\r')
        # if self.firmware_buf[i][-1]!='\r':
          # self.firmware_buf[i]+='\r'
# #      print("file:", self.firmware_buf)
      # self.fptr=0
      # self.port.write(b"\r\r")
      # self.fsm_state=self.FIRMLOAD


  def test(self):
    """
    Test
    """
    print('Test')
#    self.port.write(b"?\r")
#    self.port.write(b"\xaa\x00\x00\x00\x04\x00\x01\x0b\x0c\x0d\x14\x24")
#    fr=self.frame(2, struct.pack("<B", self.TEST))
    fr=self.frame(0, 0, b"deadbeef")
#    fr=self.frame(8)
    print("frame:", fr.hex())
    self.port.write(fr)
    self.TEST = not self.TEST


  def generate(self):
    """
    Generate
    """
    print('Generate')
    fr=self.frame(10, struct.pack("<B", self.delay_sb.value())+self.short2byte2(self.pulse_length_sb.value()))
    print("frame:", fr.hex())
    self.port.write(fr)


  def onoff(self):
    """
    Onoff
    """
    print('Onoff')
    fr=self.frame(9, struct.pack("<B", not self.onoff_cb.isChecked()))
    print("frame:", fr.hex())
    self.port.write(fr)
    
    
  def crc16_ccitt(self, crc, data):
#    print("crc: {:04x}".format(crc), data)
    msb = crc >> 8
    lsb = crc & 255
    for c in data:
      x = c ^ msb
      x ^= (x >> 4)
      msb = (lsb ^ (x >> 3) ^ (x << 4)) & 255
      lsb = (x ^ (x << 5)) & 255
#      print("crc iter:", "{:02x}".format(c), c, "{:04x}".format((msb << 8) + lsb))
#    print("crc final: {:04x}".format((msb << 8) + lsb))
    return (msb << 8) + lsb

    print("crc: {:04x}".format(crc), data)
    msb = crc >> 8
    lsb = crc & 255
    for c in data:
      print("crc iter:", c)
      x = c ^ msb
      x ^= (x >> 4)
      msb = (lsb ^ (x >> 3) ^ (x << 4)) & 255
      lsb = (x ^ (x << 5)) & 255
    print("crc final: {:04x}".format((msb << 8) + lsb))
    return (msb << 8) + lsb


  def short2byte2(self, a):
    return struct.pack("<H", a) # The ">" is the byte-order (big-endian) and the "I" is the format character.
#    return byte(a & 255)+ byte((a>>8) & 255)


  def frame(self, dest, tp, payload=b""):
#    fr=b"\x00\x00"+struct.pack("<B", tp)+self.short2char2(len(payload))+payload
#  frm_no=rcv_frm_conf
    fr=struct.pack("<B", dest)+struct.pack("<B", src)+struct.pack("<B",  (frm_conf&0x0f) | (frm_no<<4))+struct.pack("<B", tp)+struct.pack("<B", len(payload))+payload
#            destination          source                      (frm_conf<<4) | frm_no & 0x0f                  type                    length                payload
    fr+=struct.pack(">H", self.crc16_ccitt(0, fr))
    ff=b''
    for c in fr:
      ff+=struct.pack("<B",c) if c!=COMM_BESC and c!=COMM_BSHIFT else struct.pack("<B", COMM_BSHIFT)+(struct.pack("<B",COMM_SESC) if c==COMM_BESC else struct.pack("<B",COMM_SSHIFT))
    return struct.pack("<B", COMM_BESC)+ff

#    fr=b"\x00\x00"+struct.pack("<B", tp)+self.short2char2(len(payload))+payload
    # self.frm_no=self.rcv_frm_conf
    # fr=struct.pack("<B", self.frm_no)+struct.pack("<B", self.rcv_frm_no)+struct.pack("<B", tp)+self.short2byte2(len(payload))+payload
    # crc=struct.pack(">H", self.crc16_ccitt(0, fr))
    # return b"\xaa"+fr+crc


  def close_app(self):
    """
    Close the app   """
    print('exit')
    if self.port.isOpen():    
      self.port.setDataTerminalReady(True)
    app.exit()

  def closeEvent(self, event):
    if self.port.isOpen():    
      self.port.setDataTerminalReady(True)
    event.accept()


if __name__ == "__main__":
    logging.basicConfig(filename="/tmp/SH_Monitor.log", format='%(asctime)s %(message)s', level=logging.INFO)
    logging.info('='*80)
    logging.info('Started by '+getpass.getuser()+' on CPU: '+socket.gethostname())
    app = QtWidgets.QApplication(sys.argv)
    capp = SH_Monitor()
    capp.show()
    res=app.exec_()
    logging.info('Normal exit, no unhandled exceptions.')
    sys.exit(res)
    
