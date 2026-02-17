declare global {
  namespace App {
    interface Locals {}
    interface PageData {
      history?: {
        ts: string;
        congestion: number; 
        prb_util: number; 
        traffic_load: number; 
        ran_energy: number; 
        carbon_intensity: number; 
        isac_quality: number; 
        mobility_rate: number;
        crisis_score: number;
        ntn_state: number;
      }[];
    }
    interface Error {}
    interface Platform {}
  }
}

export {};

