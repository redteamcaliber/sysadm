#include "sysadm-servicemanager.h"
#include "sysadm-general.h"

#include <QFile>
#include <QDir>

using namespace sysadm;
ServiceManager::ServiceManager(QString chroot, QString ip)
{
    this->chroot = chroot;
    this->ip = ip;
    //loadServices();
}

Service ServiceManager::GetService(QString serviceName)
{
  if(!services.isEmpty()){
    for(int i=0; i<services.length(); i++){
        if(services[i].Name == serviceName)
            return services[i];
    }
  }else{
    return loadServices(serviceName);
  }
  return Service(); //no service found
}

QList<Service> ServiceManager::GetServices()
{
    if(services.isEmpty()){ loadServices(); }
    return services;
}

QList<bool> ServiceManager::isRunning(QList<Service> services){
   //return list in the same order as the input list
  QList<bool> out;
  for(int i=0; i<services.length(); i++){
    if(services[i].Directory.isEmpty()){ out << false; }
    else{ out << sysadm::General::RunQuickCommand("service",QStringList() << services[i].Directory << "status"); }
  }
  return out;
}

bool ServiceManager::isRunning(Service service){ //single-item overload
  QList<bool> run = isRunning( QList<Service>() << service);
  if(!run.isEmpty()){ return run.first(); }
  else{ return false; }
}

QList<bool> ServiceManager::isEnabled(QList<Service> services){
   //return list in the same order as the input list
  QList<bool> out;
  loadRCdata();  
  //Now go through the list of services and report which ones are enabled
  for(int i=0; i<services.length(); i++){
    bool enabled = false;
    if(rcdata.contains(services[i].Tag)){ enabled = rcdata.value(services[i].Tag)=="YES"; }
    out << enabled;
  }
  return out;
}

bool ServiceManager::isEnabled(Service service){ //single-item overload
  QList<bool> use = isEnabled( QList<Service>() << service);
  if(!use.isEmpty()){ return use.first(); }
  else{ return false; }
}

bool ServiceManager::Start(Service service)
{
    if(service.Directory.isEmpty()){ return false; }
    // Start the process
    QString prog;
    QStringList args;
    bool once = !isEnabled(service);
    if ( chroot.isEmpty() ) {
      prog = "service";
      args << service.Directory;
      args << "start";
    } else {
      prog = "warden";
      args << "chroot" << ip << "service" << service.Directory << (once ? "one" : "" )+QString("start");
    }
    return General::RunQuickCommand(prog,args);
}

bool ServiceManager::Stop(Service service)
{
    if(service.Directory.isEmpty()){ return false; }
    // Start the process
    QString prog;
    QStringList args;
    bool once = !isEnabled(service);
    if ( chroot.isEmpty() ) {
      prog = "service";
      args << service.Directory;
      args << "stop";
    } else {
      prog = "warden";
      args << "chroot" << ip << "service" << service.Directory << (once ? "one" : "" )+QString("stop");
    }
    return General::RunQuickCommand(prog,args);
}

bool ServiceManager::Restart(Service service)
{
    if(service.Directory.isEmpty()){ return false; }
    QString prog;
    QStringList args;
    bool once = !isEnabled(service);
    if ( chroot.isEmpty() ) {
      prog = "service";
      args << service.Directory;
      args <<(once ? "one" : "" )+ QString("restart");
    } else {
      prog = "warden";
      args << "chroot" << ip << "service" << service.Directory << (once ? "one" : "" )+QString("restart");
    }
    return General::RunQuickCommand(prog,args);
}

bool ServiceManager::Enable(Service service)
{
    if(service.Tag.isEmpty()){ return false; }
    return General::setConfFileValue( chroot + "/etc/rc.conf", service.Tag, service.Tag + "=\"YES\"", -1);
}

bool ServiceManager::Disable(Service service)
{
    if(service.Tag.isEmpty()){ return false; }
    return General::setConfFileValue( chroot + "/etc/rc.conf", service.Tag, service.Tag + "=\"NO\"", -1);
}

Service ServiceManager::loadServices(QString name)
{
    QString tmp;
    bool valid;
    Service service;

    QStringList stringDirs;
    stringDirs << chroot + "/etc/rc.d" << chroot + "/usr/local/etc/rc.d";

    for ( QString dir: stringDirs)
    {

        QDir directory( dir );

        directory.setFilter( QDir::Files );
        directory.setSorting( QDir::Name );

        if ( directory.count() == 0 )
            return Service();

        for (unsigned int i = 0; i < directory.count(); i++ )
        {
	    if(!name.isEmpty() && directory[i]!=name){ continue; } //not the right service - go to the next one
            service = Service();

            QFile file( dir + "/" + directory[i] );
            if ( file.open( QIODevice::ReadOnly ) )
            {
                valid=false;
                service.Directory=directory[i]; //filename only
                service.Path = dir+"/"+directory[i]; //full path w/ filename
                QTextStream stream( &file );
                stream.setCodec("UTF-8");
                QString line;
                while ( !stream.atEnd() )
                {
                    line = stream.readLine(); // line of text excluding '\n'

                    if ( line.indexOf("name=") == 0)
                    {
                        valid=true;
                        tmp = line.replace("name=", "");
                        service.Name = tmp.replace('"', "");
                    }
                    if ( line.indexOf("rcvar=") == 0)
                    {
                        if ( tmp.isEmpty() )
                            continue;

                        tmp = line.replace("rcvar=", "");
                        tmp = tmp.replace('"', "");
                        tmp = tmp.replace("'", "");
                        tmp = tmp.replace("`", "");
                        tmp = tmp.replace("$(set_rcvar)", "");
                        tmp = tmp.replace("$set_rcvar", "");
                        tmp = tmp.replace("set_rcvar", "");
                        tmp = tmp.replace("${name}", "");
                        tmp = tmp.replace("_enable", "");
                        tmp = tmp.replace(" ", "");

                        if (tmp.isEmpty())
                            service.Tag = service.Name + "_enable";
                        else
                            service.Tag = tmp;

                        if ( service.Tag.indexOf("_enable") == -1 )
                            service.Tag=service.Tag + "_enable";
                        break;
                    }
                    if (line.simplified().startsWith("desc=")) {
                      service.Description = line.section("=\"",1,-1).section("\"",0,0);
                    }
                }
                file.close();

                if ( !valid || service.Tag.isEmpty() )
                    continue;

                QString cDir = dir;
                if ( ! chroot.isEmpty() )
                    cDir.replace(chroot, "");
                if ( service.Tag.indexOf("$") == 0 )
                    service.Tag = service.Directory + "_enable";
                if ( service.Name.indexOf("$") == 0 )
                    service.Name = service.Directory;
               if(!name.isEmpty() ){ return service; } //found the requested service - return it
                services << service;
                //qDebug() << "Added Service:" << cDir << service.Directory << service.Name << service.Tag;
            }
        }
    }
  return Service();
}

void ServiceManager::loadRCdata(){
  //Read all the rc.conf files in highest-priority order
  rcdata.clear();
  QStringList info = sysadm::General::RunCommand("sysrc -A").split("\n");
  for(int i=0; i<info.length(); i++){
    if(info[i].contains(": ")){
      rcdata.insert( info[i].section(": ",0,0), info[i].section(": ",1,-1) );
    }
  }
}
