// Artificial potential field (Bounded)
// This file implement the collision avoidance system based on the artificial potential field bounded.
// Bounded because the force is felt only inside a specidic radius from the center of the drone

#include <functional>
#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <ignition/math/Vector3.hh>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include "headers.h"
#include "Neighbour.h"
#include <cmath>
//for logging
#include <iostream>
#include <fstream>



namespace gazebo
{
class boid : public ModelPlugin
{

    const float k = 300; // repulsion constant
    const double mass = 2;
    int TotalNumberDrones = 8;
    ignition::math::Vector3d final_position;
    ignition::math::Vector3d actual_position;
    ignition::math::Vector3d actual_velocity;
    neighbour me;
    ignition::math::Pose3<double> pose;
    std::vector<Neighbour> avoid ;
    std::vector<Neighbour> align ;
    std::vector<Neighbour> approach ;
    std::vector<bool> sec5; //For start all the drones togheter
    std::string name;
    int n, amount, server_fd;
    float radiusAvoid = 1;
    float radiusAlign = 4;
    float radiusApproach = 6;
    clock_t tStart;
    bool  CA= true; //CollisionAvoidance (CA) se 1 il collision avoidance e' attivo se 0 non lo e'
    bool swap = true;
    gazebo::common::Time prevTime;

    std::ofstream myFile;

    void send_to_all(Message *m, int amount)
    {
        int n, len;
        for (int i = 0; i < amount; i++)
        {
            if (i != m->src)
            {
                struct sockaddr_in *server = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
                int socketfd = client_init("127.0.0.1", 7000 + i, server);
                int ret = client_send(socketfd, server, m);
                if (ret < 0)
                    printf("%s\n", strerror(errno));
                close(socketfd);
                free(server);
            }
        }
    }

    void receive(int server_fd, int amount)
    {
        Message *m;

        for (int i = 0; i < amount; i++)
        {
            
            m = server_receive(server_fd);
            if (m)
            {
                ignition::math::Vector3d positionReceived( m->x , m->y , m->z );
                double distance = actual_position.Distance(positionReceived);
                if( distance <= radiusApproach){
                    Neighbour tmp;
                    tmp.id_ = m->src;
                    tmp.x = positionReceived.X();
                    tmp.y = positionReceived.Y();
                    tmp.z = positionReceived.Z();
                    tmp.vx = m->vx;
                    tmp.vy = m->vy;
                    tmp.vz = m->vz;
                    sec5[(m->src) - 1] = m->id;
                    approach.push_back(tmp);
                    //std::cout<<"Pushed back "<< tmp.id_<<std::endl;
                    if( distance <= radiusAlign){
                        align.push_back(tmp);
                        if( distance <= radiusAvoid){
                            avoid.push_back(tmp);
                        }
                    }
                }

            }
        }
    }

public:
    void Load(physics::ModelPtr _parent, sdf::ElementPtr _sdf)
    {
        this->model = _parent;
        prevTime = this->model->GetWorld()->RealTime();
        if (_sdf->HasElement("final_position"))
            final_position = _sdf->Get<ignition::math::Vector3d>("final_position");
        else
            final_position = ignition::math::Vector3d(0, 0, 0);

        std::string world_name = this->model->GetWorld()->Name();
        //std::cout<<"World name = "<<world_name<<std::endl;
        TotalNumberDrones = std::stoi(world_name.substr(6));
        //std::cout<<"Total Number of Drones: "<<TotalNumberDrones<<std::endl;
        //initialize vectors of avoid and avoid_pntr
        avoid.resize(TotalNumberDrones);

        //Setup of this drone Agent
        name = this->model->GetName();
        n = std::stoi(name.substr(6));
        //std::cout<< "numero "<< n <<std::endl;
        me.id_ = n;

        // Listen to the update event. This event is broadcast every
        // simulation iteration.
        this->updateConnection = event::Events::ConnectWorldUpdateBegin(
            std::bind(&boid::OnUpdate, this));

        //Setup of the Server
        amount = TotalNumberDrones + 2;
        server_fd = server_init(7000 + n);
        
        //per il logging
        //std::string file = "/home/matteo/Desktop/Results/" + name +" testX_CA0.csv";
        //myFile.open(file);

        //per la sincronizzazione
        for (int i = 0; i <= TotalNumberDrones; i++)
        {
            sec5.push_back(false);
        }
        tStart = clock();
    }

    // Called by the world update start event
public:
    void OnUpdate()
    {
         
        
        // 0.0 - UPDATE MY POS and VEL
        pose = this->model->WorldPose();
        actual_position = pose.Pos();
        //myFile<<pose.Pos().X()<<","<<pose.Pos().Y()<<","<<pose.Pos().Z()<<","<<n<<std::endl;
        me.x = pose.Pos().X();
        me.y = pose.Pos().Y();
        me.z = pose.Pos().Z();
        
        //syncronization for start
        bool go = true;
        //for (int i = 0; i < TotalNumberDrones; i++)
        //{
        //   if (!sec5[i])
        //       go = false;
        //}
        
        ignition::math::Vector3d maxVelocity;
        if (go)
        {
            maxVelocity = (final_position - actual_position).Normalize()*50;
            me.vx = maxVelocity.X();
            me.vy = maxVelocity.Y();
            me.vz = maxVelocity.Z();
        }
        else
            maxVelocity = final_position * 0;

        // 0.1 - SEND POS AND VEL
        Message m;
        m.src = n;
        if (((double)(clock() - tStart) / CLOCKS_PER_SEC) > 0.3)
        {
            sec5[n - 1] = true;
            m.id = 1;
        }
        else
        {
            m.id = 0;
        }
        m.x = me.x;
        m.y = me.y;
        m.z = me.z;
        m.vx = me.vx;
        m.vy = me.vy;
        m.vz = me.vz;
        send_to_all(&m, amount);
        
        // 0.2 - UPDATE OTHERS POS AND VEL
        receive(server_fd, amount);

        //COLLISION AVOIDANCE ALGORITHM HERE
        
        ignition::math::Vector3d me_position(me.x,me.y,me.z);
        ignition::math::Vector3d me_velocity(me.vx,me.vy,me.vz);
        ignition::math::Vector3d repulsion_force(0,0,0);
        ignition::math::Vector3d avoid_force(0,0,0);
        ignition::math::Vector3d align_force(0,0,0);
        ignition::math::Vector3d approach_force(0,0,0);
        //Se non ho vicini vai alla massima velocita
        bool vai_easy=false;
        if(avoid.empty())
            vai_easy=true;

        while(!avoid.empty()){
            auto agent = avoid.back();
            ignition::math::Vector3d agent_position(agent.x,agent.y,agent.z);
            double d = me_position.Distance(agent_position); //aggiungere raggio del drone 
            //repulsion_force += k*(radius/d)*(me_position-agent_position).Normalize();
            avoid_force += ((mass*mass)/(d*d))*(me_position-agent_position);
            avoid.pop_back();
        }
        while(!align.empty()){
            auto agent = align.back();
            ignition::math::Vector3d agent_velocity(agent.vx,agent.vy,agent.vz);
            //double d = me_position.Distance(agent_position); //aggiungere raggio del drone 
            //repulsion_force += k*(radius/d)*(me_position-agent_position).Normalize();
            align_force += ((mass*mass))*(me_velocity-agent_velocity);
            align.pop_back();
        }
        while(!approach.empty()){
            auto agent = approach.back();
            ignition::math::Vector3d agent_position(agent.x,agent.y,agent.z);
            double d = me_position.Distance(agent_position); //aggiungere raggio del drone 
            //repulsion_force += k*(radius/d)*(me_position-agent_position).Normalize();
            approach_force += ((mass*mass)/(d*d))*(me_position-agent_position);
            approach.pop_back();
        }
        double d = me_position.Distance(final_position);
        ignition::math::Vector3d attractive_force = -(k*(mass*200)/(d*d))*(me_position-final_position);
        std::cout<< name <<" approach force: "<< approach_force << " avoid_force: "<<avoid_force<<std::endl; 
        repulsion_force = (150*attractive_force - 150*approach_force - 150*align_force + 200*avoid_force);
        // 3 - UPDATE  

        // Time delta
        //std::cout<< name <<" repulsion force: "<< repulsion_force << "\n";
        double dt = (this->model->GetWorld()->RealTime() - prevTime).Double();
        //std::cout << "Delta t = "<<dt <<std::endl;
        prevTime = this->model->GetWorld()->RealTime();
        ignition::math::Vector3d repulsion_velocity = (repulsion_force/mass)*dt;
        //std::cout<< name <<" repulsion velocity: "<< repulsion_velocity << "\n";
        ignition::math::Vector3d newVelocity;
        newVelocity = 50*repulsion_velocity.Normalize();
        actual_velocity = newVelocity;
        //std::cout<< name <<" new velocity: "<< newVelocity << "\n";

        if(CA)
            this->model->SetLinearVel(newVelocity);
        else 
            this->model->SetLinearVel(maxVelocity); 


        srand(time(NULL)-n);
        //final_position = ignition::math::Vector3d(100,100,100);
        long randX = std::rand() % 500 -250;
        long randY = std::rand() % 500 -250;
        //long randZ = std::rand() % 500 -250;
        //std::cout << name << " " << randX << " " <<randY << " " <<randZ <<std::endl;
        final_position.X(final_position.X()+randX);
        final_position.Y(final_position.Y()+randY);
        //final_position.Z(final_position.Z()+randZ);
        //if((final_position-ignition::math::Vector3d(0,0,10)).Length() > 10 ){
        //        final_position = -final_position;
        //}else
        //{
        //    final_position = actual_position+5;
        //}
        
        //std::cout<<name << " final position: "<<final_position<<std::endl;
        //std::cout<<name << " final position: "<<actual_velocity<<std::endl;
        
    }

    // Pointer to the model
private:
    physics::ModelPtr model;

    // Pointer to the update event connection
private:
    event::ConnectionPtr updateConnection;
};

// Register this plugin with the simulator
GZ_REGISTER_MODEL_PLUGIN(boid)
} // namespace gazebo