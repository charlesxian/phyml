/*

PhyML:  a program that  computes maximum likelihood phylogenies from
DNA or AA homologous sequences.

Copyright (C) Stephane Guindon. Oct 2003 onward.

All parts of the source except where indicated are distributed under
the GNU public licence. See http://www.opensource.org for details.

*/


/* Routines for molecular dating */


#include "date.h"

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int DATE_Main(int argc, char **argv)
{
  option *io;

  io = Get_Input(argc,argv);
  Free(io);
  return(0);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

void DATE_XML(char *xml_filename)
{
  FILE *fp_xml_in;
  xml_node *xnd,*xnd_dum,*xnd_cal,*xroot;
  t_tree *mixt_tree,*tree;
  phydbl low,up,*res;
  char *clade_name;
  int seed;
  
  mixt_tree = XML_Process_Base(xml_filename);
  assert(mixt_tree);

  mixt_tree->rates = RATES_Make_Rate_Struct(mixt_tree->n_otu);
  RATES_Init_Rate_Struct(mixt_tree->rates,NULL,mixt_tree->n_otu);

  tree = mixt_tree;
  do
    {
      // All rate stuctures point to the same object
      tree->rates = mixt_tree->rates;
      tree = tree->next;
    }
  while(tree);

  

  fp_xml_in = fopen(xml_filename,"r");
  if(!fp_xml_in)
    {
      PhyML_Printf("\n== Could not find the XML file '%s'.\n",xml_filename);
      Exit("\n");
    }

  /* xroot = XML_Load_File(fp_xml_in); */
  xroot = mixt_tree->xml_root;

  if(xroot == NULL)
    {
      PhyML_Printf("\n== Encountered an issue while loading the XML file.\n");
      Exit("\n");
    }
  
  // Looking for calibration node(s)
  xnd = XML_Search_Node_Name("calibration",YES,xroot);

  if(xnd == NULL)
    {
      PhyML_Printf("\n== No calibration information seems to be provided.");
      PhyML_Printf("\n== Please amend your XML file. \n");
      Exit("\n");
    }
  else
    {
      if(XML_Search_Node_Name("upper",NO,xnd->child) == NULL && XML_Search_Node_Name("lower",NO,xnd->child) == NULL)
	{
	  PhyML_Printf("\n== There is no calibration information provided. \n");
	  PhyML_Printf("\n== Please check your data. \n");
	  Exit("\n");
	}
    }
  
  MIXT_Check_Model_Validity(mixt_tree);
  MIXT_Init_Model(mixt_tree);
  Print_Data_Structure(NO,stdout,mixt_tree);
  tree = MIXT_Starting_Tree(mixt_tree);
  Copy_Tree(tree,mixt_tree);
  Free_Tree(tree);
  MIXT_Connect_Cseqs_To_Nodes(mixt_tree);
  MIXT_Init_T_Beg(mixt_tree);
  MIXT_Chain_Edges(mixt_tree);
  MIXT_Chain_Nodes(mixt_tree);
  Add_Root(mixt_tree->a_edges[0],mixt_tree);  
  Prepare_Tree_For_Lk(mixt_tree);
  MIXT_Chain_All(mixt_tree);
  MIXT_Check_Edge_Lens_In_All_Elem(mixt_tree);
  MIXT_Turn_Branches_OnOff_In_All_Elem(ON,mixt_tree);
  MIXT_Check_Invar_Struct_In_Each_Partition_Elem(mixt_tree);
  MIXT_Check_RAS_Struct_In_Each_Partition_Elem(mixt_tree);

  
  xnd = xroot->child;
  assert(xnd);
  do
    {
      if(!strcmp(xnd->name,"calibration")) // Found a XML node <calibration>.
	{
          // TO DO: make sure calibs are shared across partition elements -> need to write chain function to
          // call once the calib struct on the first mixt_tree is initialized.
          /* mixt_tree->rates->tot_num_cal++; */
	  /* if (mixt_tree->rates->calib == NULL) mixt_tree->rates->calib = Make_Calib(mixt_tree->n_otu); */

	  low = -BIG;
	  up  = BIG;

	  xnd_dum = XML_Search_Node_Name("lower",YES,xnd);
	  if(xnd_dum != NULL) low = String_To_Dbl(xnd_dum->value); 

	  xnd_dum = XML_Search_Node_Name("upper",YES,xnd);
	  if(xnd_dum != NULL) up = String_To_Dbl(xnd_dum->value);
          
          xnd_cal = xnd->child;
          do
            {
              if(!strcmp("appliesto",xnd_cal->name)) 
                {
                  clade_name = XML_Get_Attribute_Value(xnd_cal,"clade.id");
                  
                  if(!clade_name)
                    {
                      PhyML_Printf("\n== Attribute 'value=CLADE_NAME' is mandatory");
                      PhyML_Printf("\n== Please amend your XML file accordingly.");
                      Exit("\n");
                    }
                  
                  if(strcmp("root",clade_name))
                    {
                      xml_node *xnd_clade;

                      xnd_clade = XML_Search_Node_Generic("clade","id",clade_name,YES,xroot);

                      if(xnd_clade != NULL) // found clade with a given name
                        {
                          char **xclade,**clade;
                          int clade_size,nd_num;
                          t_cal *cal;
                          int i;
                          
                          xclade     = XML_Read_Clade(xnd_clade->child,mixt_tree);
                          clade_size = XML_Number_Of_Taxa_In_Clade(xnd_clade->child);
                          // TO DO: chain all calibrations
                          cal        = Make_Calibration();

                          clade = (char **)mCalloc(clade_size,sizeof(char *));
                          For(i,clade_size) clade[i] = (char *)mCalloc(strlen(xclade[i])+1,sizeof(char));
                          For(i,clade_size) strcpy(clade[i],xclade[i]);
                          
                          Init_Calibration(cal);

                          mixt_tree->rates->a_cal[mixt_tree->rates->n_cal] = cal;
                          mixt_tree->rates->n_cal++;
                          
                          cal->is_primary   = YES;
                          cal->target_tax   = clade;
                          cal->n_target_tax = clade_size;
                          cal->lower        = low;
                          cal->upper        = up;

                          nd_num = Find_Clade(clade,clade_size,mixt_tree);

                          cal->target_tip = Make_Target_Tip(cal->n_target_tax);
                          Init_Target_Tip(cal,mixt_tree);

                          PhyML_Printf("\n. Node number to which calibration [%s] applies to is [%d]",clade_name,nd_num);                          
                          PhyML_Printf("\n. Lower bound set to: %15f time units.",low);
                          PhyML_Printf("\n. Upper bound set to: %15f time units.",up);
                          PhyML_Printf("\n. .......................................................................");

                          Free(xclade);
                        }
                      else
                        {
                          PhyML_Printf("\n== Calibration information for clade [%s] was not found.", clade_name);
                          PhyML_Printf("\n== Err. in file %s at line %d\n",__FILE__,__LINE__);
                          Exit("\n");
                        }                      
                    }
                }
              xnd_cal = xnd_cal->next;
            }
          while(xnd_cal != NULL);
        }
      xnd = xnd->next;
    }
  while(xnd != NULL);
  
  seed = (mixt_tree->io->r_seed < 0)?(time(NULL)):(mixt_tree->io->r_seed);
  srand(seed);
  mixt_tree->io->r_seed = seed;
  PhyML_Printf("\n. Seed: %d",seed);

  MIXT_Chain_Cal(mixt_tree);

  mixt_tree->rates->bl_from_rt = YES;
  mixt_tree->rates->model      = GAMMA;

  res = DATE_MCMC(mixt_tree);


  // Cleaning up...
  RATES_Free_Rates(mixt_tree->rates);
  RATES_Free_Rates(mixt_tree->ghost_tree->rates);
  MCMC_Free_MCMC(mixt_tree->mcmc);
  MCMC_Free_MCMC(mixt_tree->ghost_tree->mcmc);
  Free_Mmod(mixt_tree->mmod);
  Free_Spr_List(mixt_tree);
  Free_Triplet(mixt_tree->triplet_struct);
  Free_Tree_Pars(mixt_tree);
  Free_Tree_Lk(mixt_tree);

  if(mixt_tree->io->fp_out_trees)      fclose(mixt_tree->io->fp_out_trees);
  if(mixt_tree->io->fp_out_tree)       fclose(mixt_tree->io->fp_out_tree);
  if(mixt_tree->io->fp_out_stats)      fclose(mixt_tree->io->fp_out_stats);
  if(mixt_tree->io->fp_out_json_trace) fclose(mixt_tree->io->fp_out_json_trace);
  Free_Input(mixt_tree->io);


  tree = mixt_tree;
  do
    {
      Free_Calign(tree->data);
      tree = tree->next_mixt;
    }
  while(tree);

  tree = mixt_tree;
  do
    {
      Free_Optimiz(tree->mod->s_opt);
      tree = tree->next;
    }
  while(tree);

  
  Free_Model_Complete(mixt_tree->mod);
  Free_Model_Basic(mixt_tree->mod);
  Free_Tree(mixt_tree->ghost_tree);  
  Free_Tree(mixt_tree);  
  Free(res);
  XML_Free_XML_Tree(xroot);
  fclose(fp_xml_in);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Update t_prior_min and t_prior_max on a given ranked tree
// given (primary and secondary) calibration information.
// Make sure secondary and primary calibration are up-to-date
void DATE_Update_T_Prior_MinMax(t_tree *tree)
{
  int i,j,*rk;

  rk = tree->rates->t_rank;

  for(i=tree->n_otu;i<2*tree->n_otu-1;i++) // All internal nodes 
    {
      tree->rates->t_prior_max[i] = 0.0;
      tree->rates->t_prior_min[i] = -INFINITY;

      if(tree->a_nodes[i]->n_cal > 0) // Primary calibration found on that node
        {
          For(j,tree->a_nodes[i]->n_cal)
            {
              tree->rates->t_prior_max[i] = MIN(tree->rates->t_prior_max[i],tree->a_nodes[i]->cal[j]->upper);
              tree->rates->t_prior_min[i] = MAX(tree->rates->t_prior_min[i],tree->a_nodes[i]->cal[j]->lower);
            }
        }
    }

  // TO DO: chain t_rank
  TIMES_Update_Node_Ordering(tree);

  For(i,tree->n_otu-1)
    {
      for(j=i+1;j<tree->n_otu-1;j++)
        {
          if(tree->rates->t_prior_min[rk[j]] < tree->rates->t_prior_min[rk[i]])
            tree->rates->t_prior_min[rk[j]] = tree->rates->t_prior_min[rk[i]];

          if(tree->rates->t_prior_max[rk[i]] > tree->rates->t_prior_max[rk[j]])
            tree->rates->t_prior_max[rk[i]] = tree->rates->t_prior_max[rk[j]];
        }
    }

  /* printf("\n. min:%f max:%f rk: %d n_cal: %d low: %f up: %f", */
  /*        tree->rates->t_prior_min[tree->n_root->num], */
  /*        tree->rates->t_prior_max[tree->n_root->num], */
  /*        tree->rates->t_rank[tree->n_root->num], */
  /*        tree->a_nodes[tree->n_root->num]->n_cal, */
  /*        tree->a_nodes[tree->n_root->num]->n_cal > 0 ? tree->a_nodes[tree->n_root->num]->cal[0]->lower : -1, */
  /*        tree->a_nodes[tree->n_root->num]->n_cal > 0 ? tree->a_nodes[tree->n_root->num]->cal[0]->upper : -1); */
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

void DATE_Assign_Primary_Calibration(t_tree *tree)
{
  int i,j,idx,node_num;

  For(i,tree->rates->n_cal) tree->rates->a_cal[i]->target_nd = NULL;
  
  For(i,2*tree->n_otu-1) 
    For(j,MAX_N_CAL) 
    {
      tree->a_nodes[i]->cal[j] = NULL;
      tree->a_nodes[i]->n_cal  = 0;
    }
  
  For(i,tree->rates->n_cal)
    {
      node_num = Find_Clade(tree->rates->a_cal[i]->target_tax,
                            tree->rates->a_cal[i]->n_target_tax,
                            tree);
      
      idx = tree->a_nodes[node_num]->n_cal;
      tree->a_nodes[node_num]->cal[idx] = tree->rates->a_cal[i];
      tree->a_nodes[node_num]->cal[idx]->target_nd = tree->a_nodes[node_num];
      tree->a_nodes[node_num]->n_cal++;
      

      if(tree->a_nodes[node_num]->n_cal == MAX_N_CAL)
        {
          PhyML_Printf("\n== A node cannot have more than %d calibration",MAX_N_CAL); 
          PhyML_Printf("\n== constraints attached to it. Feel free to increase the"); 
          PhyML_Printf("\n== value of the variable MAX_N_CAL in utilities.h if");
          PhyML_Printf("\n== necessary.");
          Exit("\n");
        }
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Return splitted calibration intervals. Make sure all primary
// and secondary calibration intervals are up-to-date.
phydbl *DATE_Splitted_Calibration(t_tree *tree)
{
  phydbl *minmax,*splitted_cal,buff;
  int i,len,done;

  // One t_prior_min and one t_prior_max per internal nodes except root, so 
  // 2 x # of internal nodes boundaries in total at most.
  minmax = (phydbl *)mCalloc(2*(tree->n_otu-2),sizeof(phydbl));
  For(i,2*(tree->n_otu-2)) minmax[i] = +INFINITY;
  splitted_cal = (phydbl *)mCalloc((int)(4*tree->n_otu-10),sizeof(phydbl));


  len = 0;
  for(i = tree->n_otu; i < 2*tree->n_otu-1; i++)
    {
      if(tree->a_nodes[i] != tree->n_root)
        {
          minmax[len]   = MAX(tree->rates->t_prior_min[i],tree->rates->nd_t[tree->n_root->num]);
          minmax[len+1] = tree->rates->t_prior_max[i];        
          len+=2;
        }
    }

  
  // Bubble sort of all these times in increasing order
  do
    {
      done = YES;
      For(i,len-1) 
        {
          if(minmax[i] > minmax[i+1])
            {
              buff        = minmax[i];
              minmax[i]   = minmax[i+1];
              minmax[i+1] = buff;
              done = NO;
            }
        }
    }
  while(done == NO);

  For(i,len-1) assert(!(minmax[i] > minmax[i+1]));
  
  // Remove ties
  For(i,len-1) 
    if(Are_Equal(minmax[i],minmax[i+1],1.E-6) == YES) 
      minmax[i] = 0.0;

  // Sort again to effectively remove ties
  do
    {
      done = YES;
      For(i,len-1) 
        {
          if(minmax[i] > minmax[i+1])
            {
              buff        = minmax[i];
              minmax[i]   = minmax[i+1];
              minmax[i+1] = buff;
              done = NO;
            }
        }
    }
  while(done == NO);

  splitted_cal[0] = minmax[0];
  len = 1;
  for(i = 1; i < 2*(tree->n_otu-2); i++)                                        
    {
      splitted_cal[len]   = minmax[i];
      if(len+1 < 4*tree->n_otu-10) splitted_cal[len+1] = minmax[i];
      len+=2;
    }

  /* For(i,4*tree->n_otu-10) PhyML_Printf("\n. split -- %3d %12f",i,splitted_cal[i]); */

  Free(minmax);
   
  return splitted_cal;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

phydbl DATE_J_Sum_Product(t_tree *tree)
{
  phydbl prod, total,*splitted_cal;
  int fact,idx,ans,rk;
  
  DATE_Assign_Primary_Calibration(tree);
  DATE_Update_T_Prior_MinMax(tree);

  splitted_cal = DATE_Splitted_Calibration(tree);
  
  ans   = 0;
  total = 0.0;
  idx   = 0;
  rk    = 1;
  do
    {
      prod = 1.0;
      fact = 1;
      ans = DATE_J_Sum_Product_Pre(tree->a_nodes[tree->rates->t_rank[rk]], // Oldest node after root (as rk=1)
                                   idx,
                                   -1,
                                   prod,fact,&total,splitted_cal,rk,tree);
      idx+=2;
    }
  while(ans != 1);

  Free(splitted_cal);

  return(total);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int DATE_J_Sum_Product_Pre(t_node *d, int split_idx_d, int split_idx_a, phydbl prod, int fact, phydbl *total, phydbl *splitted_cal, int rk, t_tree *tree)
{
  int ans,idx;

  ans = DATE_Is_Split_Accessible(d,split_idx_d,splitted_cal,tree);

  switch(ans)
    {
    case 1 : // split interval is younger than t_prior_max. No need to go further.
      {
        return ans;
        break;
      }
    case 0 : // split interval is within [t_prior_min,t_prior_max]
      {
        int local_ans;

        // Calculate J for this time interval
        prod *= DATE_J(tree->rates->birth_rate, 
                       tree->rates->death_rate,
                       FABS(splitted_cal[split_idx_d+1]),
                       FABS(splitted_cal[split_idx_d]));
        
        // Remove factorial term from current product
        prod *= fact;
        
        if(split_idx_d == split_idx_a) fact++;
        else fact = 1;
        
        prod /= fact;
                
        if(tree->rates->t_rank[tree->n_otu-2] == d->num) // Youngest internal node 
          {
            (*total) += prod;
            return 0;
          }

        idx = split_idx_d;
        do
          {
            local_ans = DATE_J_Sum_Product_Pre(tree->a_nodes[tree->rates->t_rank[rk+1]],
                                               idx,
                                               split_idx_d,
                                               prod,fact,total,splitted_cal,rk+1,tree);
            idx+=2;
          }
        while(local_ans == 0);

        break;
      }
    case -1 : // split interval is older than t_prior_min. Move forward.
      {
        int local_ans;
        // Advance to younger split intervals and stop once you're in
        idx = split_idx_d+2;
        do
          {
            local_ans = DATE_J_Sum_Product_Pre(d,
                                               idx,
                                               idx+1,
                                               prod,fact,total,splitted_cal,rk,tree);
            idx+=2;
          }
        while(local_ans == -1);
        break;
      }
    }

  return ans;
}
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int DATE_Is_Split_Accessible(t_node *d, int which, phydbl *splitted_cal, t_tree *tree)
{
  phydbl eps;

  assert(d->tax == NO);

  eps = FABS(tree->rates->t_prior_min[d->num]) / 1.E+6;

  assert(eps > MDBL_MIN);

  // Upper and lower bound of splitted calibration interval are equal to zero
  if(Are_Equal(splitted_cal[which],0.0,eps) && Are_Equal(splitted_cal[which+1],0.0,eps)) return +1;


  if(Are_Equal(tree->rates->t_prior_min[d->num],splitted_cal[which],eps)   ||
     Are_Equal(tree->rates->t_prior_max[d->num],splitted_cal[which+1],eps) ||
     (tree->rates->t_prior_min[d->num] < splitted_cal[which] && 
      tree->rates->t_prior_max[d->num] > splitted_cal[which+1]))                   return  0; // splitted interval is within [t_prior_min,t_prior_max]
  else if(Are_Equal(tree->rates->t_prior_max[d->num],splitted_cal[which],eps) ||
          splitted_cal[which] > tree->rates->t_prior_max[d->num])                  return +1; // splitted interval is younger than [t_prior_min,t_prior_max]
  else if(Are_Equal(tree->rates->t_prior_min[d->num],splitted_cal[which+1],eps) ||
          splitted_cal[which+1] < tree->rates->t_prior_min[d->num])                return -1; // splitted interval is older than [t_prior_min,t_prior_max]
  else
    {
      PhyML_Printf("\n== d->num: %d d->tax: %d",d->num,d->tax);
      PhyML_Printf("\n== t_prior_min: %f t_prior_max: %f",
                   tree->rates->t_prior_min[d->num],
                   tree->rates->t_prior_max[d->num]);
      PhyML_Printf("\n== splitted_cal_min: %f splitted_cal_max: %f",
                   splitted_cal[which],
                   splitted_cal[which+1]);      
      PhyML_Printf("\n");
      assert(FALSE); // splitted interval cannot be partially overlapping [t_prior_min,t_prior_max]
    }
  return(0);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

phydbl DATE_J(phydbl birth_r, phydbl death_r, phydbl t_min, phydbl t_pls)
{
  phydbl d,b,J;
  assert(t_pls > t_min);
  d = death_r;
  b = birth_r;
  J = (b-d)*(EXP(t_min*d+t_pls*b) - EXP(t_min*b+t_pls*d));
  J /= ((b*EXP(t_min*b)-d*EXP(t_min*d)) * (b*EXP(t_pls*b)-d*EXP(t_pls*d)));
  /* printf("  J : %f",J); */
  return(J);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int DATE_Check_Calibration_Constraints(t_tree *tree)
{
  int i,j;
  phydbl lower,upper;

  lower = upper = -1.;

  For(i,2*tree->n_otu-1)
    {
      if(tree->a_nodes[i]->n_cal > 1)
        {
          lower = tree->a_nodes[i]->cal[0]->lower;
          upper = tree->a_nodes[i]->cal[0]->upper;
          for(j=1; j < tree->a_nodes[i]->n_cal; j++)
            {
              lower = MAX(lower,tree->a_nodes[i]->cal[j]->lower);
              upper = MIN(upper,tree->a_nodes[i]->cal[j]->upper);
              if(upper < lower) 
                {
                  /* PhyML_Printf("\n. Inconsistency detected on node %d",i); */
                  return 0; 
                }
            }
        }
    }
  return 1;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int DATE_Check_Time_Constraints(t_tree *tree)
{
  int i;

  For(i,2*tree->n_otu-1)
    {
      if(tree->a_nodes[i] != tree->n_root && tree->a_nodes[i]->tax == NO)
        {
          if(tree->rates->nd_t[i] > tree->rates->t_prior_max[i] ||
             tree->rates->nd_t[i] < tree->rates->t_prior_min[i])
            {
              return 0;
            }
        }
    }
  return 1;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

phydbl *DATE_MCMC(t_tree *tree)
{
  t_mcmc *mcmc;
  char *s_tree;
  int move, n_vars, i, adjust_len;
  phydbl u;
  phydbl *res;
  FILE *fp_stats,*fp_tree;
  int t_beg;

  t_beg = (int)time(NULL);

  fp_stats = tree->io->fp_out_stats;
  fp_tree = tree->io->fp_out_tree;

  TIMES_Randomize_Tree_With_Time_Constraints(tree->rates->a_cal[0], tree);

  mcmc = MCMC_Make_MCMC_Struct();
  tree->mcmc = mcmc;
  MCMC_Init_MCMC_Struct(NULL,NULL,mcmc);
  MCMC_Complete_MCMC(mcmc,tree);
  
  MCMC_Randomize_Birth(tree);
  MCMC_Randomize_Death(tree);
  MCMC_Randomize_Clock_Rate(tree);

  PhyML_Printf("\n. birth: %f death: %f clock: %f",
               tree->rates->birth_rate,
               tree->rates->death_rate,
               tree->rates->clock_r);

  tree->rates->bl_from_rt = YES;  
  RATES_Update_Cur_Bl(tree);

  n_vars                = 10;
  adjust_len            = 1E+6;
  mcmc->sample_interval = 100;
  mcmc->chain_len       = 1E+7;

  res = (phydbl *)mCalloc(tree->mcmc->chain_len / tree->mcmc->sample_interval * n_vars,sizeof(phydbl));
  
  Set_Both_Sides(YES,tree);
  Lk(NULL,tree);
  TIMES_Lk_Times(tree);
  RATES_Lk_Rates(tree);

  PhyML_Printf("\n. log(Pr(Seq|Tree)) = %f",tree->c_lnL);
  PhyML_Printf("\n. log(Pr(Tree)) = %f",tree->rates->c_lnL_times);
  

  tree->ghost_tree = Make_Tree_From_Scratch(tree->n_otu,tree->data);
  Copy_Tree(tree,tree->ghost_tree);
  tree->ghost_tree->rates = RATES_Make_Rate_Struct(tree->n_otu);
  RATES_Init_Rate_Struct(tree->ghost_tree->rates,NULL,tree->n_otu);
  RATES_Copy_Rate_Struct(tree->rates,tree->ghost_tree->rates,tree->n_otu);
  RATES_Duplicate_Calib_Struct(tree,tree->ghost_tree);
  MIXT_Chain_Cal(tree->ghost_tree);
  TIMES_Lk_Times(tree->ghost_tree);
  PhyML_Printf("\n. log(Pr(Ghost Tree)) = %f",tree->ghost_tree->rates->c_lnL_times);
  mcmc = MCMC_Make_MCMC_Struct();
  tree->ghost_tree->mcmc = mcmc;
  MCMC_Init_MCMC_Struct(NULL,NULL,mcmc);
  MCMC_Complete_MCMC(mcmc,tree->ghost_tree);


  PhyML_Fprintf(fp_stats,"\n%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s",
                "sample",
                "lnL(seq)",
                "lnL(times)",
                "birth",
                "death",
                "clock",
                "root",
                "tstv");
  fflush(NULL);


  For(i,mcmc->n_moves) tree->mcmc->start_ess[i] = YES;
  Set_Both_Sides(NO,tree);
  mcmc->always_yes = NO;
  move             = -1;

  do
    {

      /* tree->mcmc->adjust_tuning[i] = NO; */
      if(mcmc->run > adjust_len) For(i,mcmc->n_moves) tree->mcmc->adjust_tuning[i] = NO;

      if(tree->c_lnL < UNLIKELY + 0.1)
        {
          PhyML_Printf("\n== Move '%s' failed\n",tree->mcmc->move_name[move]);
          assert(FALSE);
        }

      u = Uni();

      For(move,tree->mcmc->n_moves) if(tree->mcmc->move_weight[move] > u-1.E-10) break;

      assert(!(move == tree->mcmc->n_moves));      

      
      /* PhyML_Printf("\n== Move '%s' %f",tree->mcmc->move_name[move],tree->ghost_tree->rates->c_lnL_times); */

      
      if(!strcmp(tree->mcmc->move_name[move],"clock"))               MCMC_Clock_R(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"birth_rate"))     MCMC_Birth_Rate(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"death_rate"))     MCMC_Death_Rate(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"tree_height"))    MCMC_Tree_Height(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"times"))          MCMC_Time_All(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"spr"))            MCMC_Prune_Regraft(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"spr_local"))      MCMC_Prune_Regraft_Local(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"updown_t_cr"))    MCMC_Updown_T_Cr(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"updown_t_br"))    MCMC_Updown_T_Br(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"subtree_rates"))  MCMC_Subtree_Rates(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"kappa"))          MCMC_Kappa(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"ras"))            MCMC_Rate_Across_Sites(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"nu"))             MCMC_Nu(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"subtree_height")) MCMC_Subtree_Height(tree);
      else if(!strcmp(tree->mcmc->move_name[move],"time_slice"))     MCMC_Time_Slice(tree);

      else if(!strcmp(tree->mcmc->move_name[move],"br_rate"))
      	{
      	  Set_Both_Sides(YES,tree);
      	  if(tree->eval_alnL == YES) Lk(NULL,tree);
      	  Set_Both_Sides(NO,tree);
          MCMC_One_Rate(tree->n_root,tree->n_root->v[1],YES,tree);
          MCMC_One_Rate(tree->n_root,tree->n_root->v[2],YES,tree);
      	}
      else continue;

     
      if(!TIMES_Check_Node_Height_Ordering(tree))
        {
          PhyML_Printf("\n== move: %s",tree->mcmc->move_name[move]);
          Exit("\n");
        }


      tree->mcmc->run++;
      MCMC_Get_Acc_Rates(tree->mcmc);

      if(!(tree->mcmc->run%tree->mcmc->sample_interval))
        {
          PhyML_Printf("\n. lnL: [%10.2f -- %10.2f -- %10.2f -- %10.2f] clock: %12f root age: %12f [time: %7d sec]",
                       tree->c_lnL,
                       tree->rates->c_lnL_times,
                       tree->rates->c_lnL_rates,
                       tree->ghost_tree->rates->c_lnL_times,
                       tree->rates->clock_r,
                       tree->rates->nd_t[tree->n_root->num],
                       (int)time(NULL) - t_beg);

          PhyML_Fprintf(fp_stats,"\n%6d\t%9.1f\t%9.1f\t%12G\t%12G\t%12G\t%12G\t%12G",
                        tree->mcmc->run,
                        tree->c_lnL,
                        tree->rates->c_lnL_times,
                        tree->rates->birth_rate,
                        tree->rates->death_rate,
                        tree->rates->clock_r,
                        tree->rates->nd_t[tree->n_root->num],
                        tree->mod->kappa->v);

          Time_To_Branch(tree);
          tree->bl_ndigits = 1;
          s_tree = Write_Tree(tree,NO);
          tree->bl_ndigits = 7;
          PhyML_Fprintf(fp_tree,"\n%s",s_tree);
          Free(s_tree);

          fflush(NULL);

          tree->mcmc->sample_num++;
        }
    }
  while(tree->mcmc->run < tree->mcmc->chain_len);


  return(res);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Update the list of nodes that are younger than lim
void DATE_List_Of_Nodes_Younger_Than(t_node *a, t_node *d, phydbl lim, t_ll **list, t_tree *tree)
{
  if(tree->rates->nd_t[d->num] > lim) Push_Bottom_Linked_List(d,list);
  
  if(d->tax == YES) return;
  else
    {
      int i;

      For(i,3)
        if(d->v[i] != a && d->b[i] != tree->e_root)
          DATE_List_Of_Nodes_Younger_Than(d,d->v[i],lim,list,tree);            
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Update the list of nodes that are younger than lim with direct ancestors 
// not younger than lim
void DATE_List_Of_Nodes_And_Ancestors_Younger_Than(t_node *a, t_node *d, phydbl lim, t_ll **list, t_tree *tree)
{
  if(tree->rates->nd_t[d->num] > lim && tree->rates->nd_t[a->num] > lim) Push_Bottom_Linked_List(d,list);
  
  if(d->tax == YES) return;
  else
    {
      int i;

      For(i,3)
        if(d->v[i] != a && d->b[i] != tree->e_root)
          DATE_List_Of_Nodes_And_Ancestors_Younger_Than(d,d->v[i],lim,list,tree);            
    }
}


//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// List of valid regraft nodes, taking into account calibration
// constraints. The subtree (defined by prune and prune_daughter
// will be re-attached *on top of* one of the nodes in this list
// (as opposed to *on one of the sister edges below*).
t_ll *DATE_List_Of_Regraft_Nodes(t_node *prune, t_node *prune_daughter, t_tree *tree)
{
  t_node *n,*m;
  int i,j;
  t_ll *out,*in,*ll;
  phydbl maxmin;
  int is_clade_affected;

  /* printf("\n\n. Begin"); */
  n = NULL;
  m = NULL;
  maxmin = -INFINITY;
  in = NULL;
  out = NULL;
  is_clade_affected = NO;

  /* Print_Node(tree->n_root,tree->n_root->v[1],tree); */
  /* Print_Node(tree->n_root,tree->n_root->v[2],tree);               */

  /* PhyML_Printf("\n. prune: %d prune_daughter: %d [%f %f] v0: %d %f  v1: %d %f  v2: %d %f", */
  /*              prune->num, */
  /*              prune_daughter->num, */
  /*              tree->rates->nd_t[prune->num], */
  /*              tree->rates->nd_t[prune_daughter->num], */
  /*              prune->v[0]->num, */
  /*              tree->rates->nd_t[prune->v[0]->num], */
  /*              prune->v[1]->num, */
  /*              tree->rates->nd_t[prune->v[1]->num], */
  /*              prune->v[2]->num, */
  /*              tree->rates->nd_t[prune->v[2]->num]); */

  // Find the oldest LCA of calibrated sets among the nodes between
  // prune and root. These clades might see the position of their 
  // LCA change.
  n = prune;
  while(n)
    {
      For(i,tree->rates->n_cal)
        {
          // That node is the LCA of calibration a_cal[i]
          if(n == tree->rates->a_cal[i]->target_nd)
            {
              /* printf("\n. n: %d lo:%f up:%f min:%f max:%f",n->num,tree->rates->a_cal[i]->lower,tree->rates->a_cal[i]->upper,tree->rates->t_prior_min[n->num],tree->rates->t_prior_max[n->num]); */
              is_clade_affected = NO;
              For(j,tree->rates->a_cal[i]->n_target_tax)
                {
                  m = tree->rates->a_cal[i]->target_tip[j];
                  do
                    {
                      if(m == prune_daughter) 
                        {
                          is_clade_affected = YES;
                          break;
                        }
                      m = m->anc;
                    }
                  while(m);

                  if(is_clade_affected == YES) break;
                }
              
              // Maximum of the lower bounds for calibration intervals
              if(is_clade_affected == YES) maxmin = MAX(maxmin,tree->rates->a_cal[i]->lower);
            }
        }

      n = n->anc;
    }

  /* printf("\n. maxmin: %f t: %f %f",maxmin,tree->rates->nd_t[prune->num],TIMES_Lk_Times(tree)); */
  assert(maxmin < tree->rates->nd_t[prune->num]);

  // Find the oldest internal node within intervals defined by 
  // calibrations affected by the pruning.
  n = prune_daughter;
  while(n->anc && !(tree->rates->nd_t[n->anc->num] < maxmin))
    {      
      n = n->anc;
      assert(n);
    }
  
  /* printf("\n. Apical: %d",n->num); fflush(NULL); */
     
  // List all nodes younger than this apical node
  DATE_List_Of_Nodes_Younger_Than(n->anc,n,-INFINITY,&in,tree);
  
  /* /\* printf("\n. Younger"); fflush(NULL); *\/ */
  /* ll = in->head; */
  /* t_node *x; */
  /* do  */
  /*   { */
  /*     x = (t_node *)ll->v; */
  /*     /\* printf("\n. In0: %p %d",ll,x->num); fflush(NULL); *\/ */
  /*     ll = ll->next;  */
  /*   }  */
  /* while(ll != NULL); */


  // Remove from that list the nodes that too young to be suitable
  // regraft points.
  n = prune_daughter;
  out = NULL;
  while(n)
    {      
      /* printf("\n. n: %d",n->num); fflush(NULL); */
      For(i,tree->rates->n_cal)
        {          
          if(n->anc && n->anc == tree->rates->a_cal[i]->target_nd)
            {
              For(j,3)
                {
                  if(n->anc->v[j] != n->anc->anc && n->anc->v[j] != n)
                    {
                      /* printf("\n. from: %d to: %d upper: %f",n->anc->num,n->anc->v[j]->num,tree->rates->a_cal[i]->upper); fflush(NULL); */
                      DATE_List_Of_Nodes_And_Ancestors_Younger_Than(n->anc,
                                                                    n->anc->v[j],
                                                                    tree->rates->a_cal[i]->upper,
                                                                    &out,
                                                                    tree);
                      break;
                    }
                }
            }
        }
      n = n->anc;
    }


  // Remove nodes that are `strictly' younger than prune_daughter
  DATE_List_Of_Nodes_And_Ancestors_Younger_Than(tree->n_root,tree->n_root->v[1],tree->rates->nd_t[prune_daughter->num],&out,tree);
  DATE_List_Of_Nodes_And_Ancestors_Younger_Than(tree->n_root,tree->n_root->v[2],tree->rates->nd_t[prune_daughter->num],&out,tree);

  // Remove nodes that are below prune_daughter (prune_daughter included)
  DATE_List_Of_Nodes_Younger_Than(prune,prune_daughter,-INFINITY,&out,tree);

  // Add prune node to the list of node that can't be targeted for regraft
  Push_Bottom_Linked_List(prune,&out);

  // Add root node as one cannot regraft above it
  Push_Bottom_Linked_List(tree->n_root,&out);

  /* /\* printf("\nx outlist: %p",out); fflush(NULL); *\/ */
  /* ll = out->head; */
  /* do */
  /*   { */
  /*     /\* x = (t_node *)ll->v; *\/ */
  /*     /\* printf("\nx Outlist %d",x->num); *\/ */
  /*     ll = ll->next; */
  /*   } */
  /* while(ll != NULL); */


  /* printf("\nx out: %p",out); fflush(NULL); */
  /* Print_List(in); */
  ll = out->head;
  do
    {
      /* x = (t_node *)ll->v; */
      /* printf("\nx Remove %d",x->num); */
      Remove_From_Linked_List(NULL,ll->v,&in);
      /* PhyML_Printf("\n. List in (in->head:%p in->tail:%p):",in->head,in->tail); */
      /* Print_List(in); */
      ll = ll->next;
    }
  while(ll != ll->tail);

  Free_Linked_List(out);

  /* ll = in->head; */
  /* do */
  /*   { */
  /*     t_node *x; */
  /*     x = (t_node *)ll->v; */
  /*     printf("\n. In1: %d",x->num); fflush(NULL); */
  /*     ll = ll->next; */
  /*   } */
  /* while(ll != NULL); */

  return(in);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

